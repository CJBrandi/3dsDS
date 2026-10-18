#include <3ds.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "protocol.h"
#include "safety.h"
#include "link.h"
#include "ui.h"
#include "wifi.h"
#include "wifi_scan.h"

#define CONFIG_DIR "sdmc:/3ds/3dsDS"
#define CONFIG_PATH CONFIG_DIR "/config.txt"
static UiPage page;
static UiPopup popup;
static unsigned network_page;
static bool show_password;
static Safety safety;
static Link link_state;
static struct sockaddr_in robot_addr;
static int sock = -1;
static uint16_t sequence;
static Feed feed;
static int feed_sock=-1;
static bool feed_connecting;
static uint64_t feed_deadline, feed_retry;
static void close_feed(void) {
    if(feed_sock>=0) close(feed_sock);
    feed_sock=-1; feed_connecting=false;
    feed_reset_stream(&feed);
}
/* The dedicated WPILib console port carries the DS prints/errors without
 * requiring joystick-descriptor or other outgoing TCP commands. */
static void poll_feed(uint64_t now,bool available) {
    if(!available) { close_feed(); return; }
    if(feed_sock<0) {
        if(now<feed_retry) return;
        feed_retry=now+3000;
        feed_sock=socket(AF_INET,SOCK_STREAM,0);
        if(feed_sock<0) return;
        if(fcntl(feed_sock,F_SETFL,O_NONBLOCK)!=0) { close_feed(); return; }
        struct sockaddr_in peer=robot_addr; peer.sin_port=htons(1741);
        int result=connect(feed_sock,(struct sockaddr *)&peer,sizeof peer);
        if(result<0 && errno!=EINPROGRESS && errno!=EWOULDBLOCK) { close_feed(); return; }
        feed_connecting=result<0; feed_deadline=now+2000;
    }
    if(feed_connecting) {
        fd_set writable,errors; FD_ZERO(&writable); FD_ZERO(&errors);
        FD_SET(feed_sock,&writable); FD_SET(feed_sock,&errors);
        struct timeval timeout={0};
        int result=select(feed_sock+1,NULL,&writable,&errors,&timeout);
        if(result<0 || now>=feed_deadline) { close_feed(); return; }
        if(!result) return;
        int error=0; socklen_t length=sizeof error;
        if(getsockopt(feed_sock,SOL_SOCKET,SO_ERROR,&error,&length)<0 || error) { close_feed(); return; }
        feed_connecting=false;
    }
    for(unsigned i=0;i<4;i++) {
        uint8_t data[1024]; ssize_t n=recv(feed_sock,data,sizeof data,0);
        if(n>0) feed_receive(&feed,data,(size_t)n);
        else {
            if(!n || (errno!=EAGAIN && errno!=EWOULDBLOCK)) {
                close_feed(); feed_retry=now+3000;
                feed_text(&feed,"\nConsole disconnected. Reconnecting...\n");
            }
            break;
        }
    }
}
static uint8_t alliance;
static unsigned team_number;
static char target[16];
static char message[80] = "Set your team number on Drive.";
static bool configured;
static uint32_t lifecycle_event;

static uint64_t now_ms(void) { return svcGetSystemTick() / (SYSCLOCK_ARM11 / 1000); }
static bool timing_fault(uint64_t now,uint64_t previous) {
    bool event=__atomic_exchange_n(&lifecycle_event,0,__ATOMIC_ACQ_REL)!=0;
    if(!event && now>=previous && now-previous<=100) return false;
    safety_disable(&safety); memset(&link_state,0,sizeof link_state);
    return true;
}
static bool transmit(const Controls *controls, uint64_t now, bool force_disabled);
static void lifecycle(APT_HookType hook, void *unused) {
    (void)unused;
    /* libctru invokes these hooks synchronously before entering HOME/sleep. */
    safety_disable(&safety);
    close_feed();
    if(wifi_setup.busy) wifi_setup_cancel();
    wifi_scan_cancel(); show_password=false;
    if(hook==APTHOOK_ONSUSPEND || hook==APTHOOK_ONSLEEP || hook==APTHOOK_ONEXIT) {
        Controls empty={{0},0,-1};
        transmit(&empty,now_ms(),true);
    }
    __atomic_store_n(&lifecycle_event, 1, __ATOMIC_RELEASE);
}
static bool set_team(unsigned team) {
    if(team<1 || team>25599) return false;
    snprintf(target,sizeof target,"10.%u.%u.2",team/100,team%100);
    struct in_addr address;
    if(inet_pton(AF_INET,target,&address)!=1) return false;
    memset(&robot_addr,0,sizeof robot_addr);
    robot_addr.sin_family=AF_INET; robot_addr.sin_port=htons(DS_PORT);
    robot_addr.sin_addr=address;
    if(!configured || team_number!=team) {
        close_feed(); feed_clear(&feed); feed_retry=0;
        feed_text(&feed,"Waiting for roboRIO console...\n");
    }
    team_number=team; configured=true;
    memset(&link_state,0,sizeof link_state); safety_disable(&safety);
    return true;
}
static void save_config(void) {
    mkdir("sdmc:/3ds", 0777); mkdir(CONFIG_DIR, 0777);
    FILE *f = fopen(CONFIG_PATH ".tmp", "w");
    if (!f) { snprintf(message, sizeof message, "SD save failed; using session settings."); return; }
    bool ok = fprintf(f, "%u %u %u %u\n", team_number, alliance, wifi_setup.slot+1, (unsigned)wifi_setup.saved_security) > 0;
    if (fclose(f) != 0) ok = false;
    if (!ok || rename(CONFIG_PATH ".tmp", CONFIG_PATH) != 0)
        snprintf(message, sizeof message, "SD save failed; using session settings.");
}
static void load_config(void) {
    unsigned team=0,a=0,slot=0,security=WIFI_UNSUPPORTED;
    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) return;
    int fields=fscanf(f, "%u %u %u %u", &team, &a, &slot, &security);
    if(fields>=3 && slot>=1 && slot<=3) {
        wifi_setup.slot=slot-1;
        memcpy(wifi_setup.ssid,wifi_setup.profiles[slot-1],33);
        wifi_setup.password_saved=wifi_setup.ssid[0]!=0;
        wifi_setup.security=fields==4 && security<=WIFI_OPEN ? (WifiSecurity)security:WIFI_UNSUPPORTED;
        wifi_setup.saved_security=wifi_setup.security;
    }
    if (fields>=2 && a <= 5 && set_team(team)) {
        alliance = a;
        snprintf(message, sizeof message, "Ready. Tap ENABLE on the Drive screen.");
    }
    fclose(f);
}
static Calendar calendar_now(void) {
    struct timeval tv; gettimeofday(&tv, NULL);
    time_t stamp = tv.tv_sec;
    struct tm *tm = gmtime(&stamp);
    Calendar c = {0};
    if (tm) {
        c.usec = tv.tv_usec; c.sec = tm->tm_sec; c.min = tm->tm_min;
        c.hour = tm->tm_hour; c.day = tm->tm_mday;
        c.month = tm->tm_mon; c.year = tm->tm_year;
    }
    return c;
}
static bool transmit(const Controls *controls, uint64_t now, bool force_disabled) {
    if (sock < 0 || !configured) return false;
    if(!link_connected(&link_state,now) || !link_state.robot.code ||
       wifi_setup.busy || wifi_scan.scanning) safety_disable(&safety);
    uint8_t data[DS_PACKET_MAX];
    Command cmd = {sequence++, safety.enabled && !force_disabled, safety.estop, alliance};
    Calendar cal = calendar_now();
    bool time_needed = !link_connected(&link_state, now) || link_state.robot.request_time;
    size_t len = ds_packet(data, sizeof data, &cmd, controls, time_needed ? &cal : NULL);
    ssize_t sent = sendto(sock, data, len, 0, (struct sockaddr *)&robot_addr, sizeof robot_addr);
    if (sent != (ssize_t)len) {
        safety_disable(&safety);
        /* Don't allow old replies to re-arm a broken send path. */
        memset(&link_state, 0, sizeof link_state);
        snprintf(message, sizeof message, "Send failed (%d). Robot disabled.", errno);
        return false;
    }
    link_sent(&link_state, cmd.sequence, now);
    return true;
}
static bool receive_status(uint64_t now) {
    /* Bounded work per frame, including on a noisy network. */
    for (int count = 0; count < 32; ++count) {
        /* One sentinel byte lets recvfrom expose oversized/truncated datagrams. */
        uint8_t data[2049]; struct sockaddr_in peer; socklen_t len = sizeof peer;
        ssize_t n = recvfrom(sock, data, sizeof data, 0, (struct sockaddr *)&peer, &len);
        if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                safety_disable(&safety); memset(&link_state, 0, sizeof link_state);
                snprintf(message, sizeof message, "Receive failed (%d).", errno);
                return true;
            }
            break;
        }
        if((size_t)n>=sizeof data) continue;
        if (configured && peer.sin_family == AF_INET &&
            peer.sin_addr.s_addr == robot_addr.sin_addr.s_addr &&
            link_receive(&link_state, data, (size_t)n, now)) {
            safety_observe_robot(&safety,&link_state.robot);
            if(safety.estop || !link_state.robot.code) {
                /* No queued recovery packet may erase a fault or re-arm enable. */
                memset(link_state.valid,0,sizeof link_state.valid);
                return true;
            }
        }
    }
    return false;
}
static Controls read_controls(uint32_t held) {
    circlePosition left = {0}, right = {0};
    hidCircleRead(&left); hidCstickRead(&right);
    Controls c = {{ds_axis(left.dx, 156, 15), ds_axis(-left.dy, 156, 15),
                   ds_axis(right.dx, 156, 15), ds_axis(-right.dy, 156, 15)}, 0, -1};
    const uint32_t keys[10] = {KEY_A, KEY_B, KEY_X, KEY_Y, KEY_L, KEY_R, KEY_ZR, KEY_ZL, KEY_SELECT, KEY_START};
    for (int i = 0; i < 10; ++i) if (held & keys[i]) c.buttons |= 1u << i;
    c.pov = ds_pov(held & KEY_DUP, held & KEY_DRIGHT, held & KEY_DDOWN, held & KEY_DLEFT);
    return c;
}
static bool neutral(const Controls *c) {
    return !c->axes[0] && !c->axes[1] && !c->axes[2] && !c->axes[3] && !c->buttons && c->pov == -1;
}
static void edit_team(void) {
    Controls empty={{0},0,-1};
    safety_disable(&safety); transmit(&empty,now_ms(),true);
    SwkbdState kb;
    swkbdInit(&kb,SWKBD_TYPE_NUMPAD,2,5);
    swkbdSetHintText(&kb,"FRC team number (1-25599)");
    char text[16]={0};
    if(configured) { snprintf(text,sizeof text,"%u",team_number); swkbdSetInitialText(&kb,text); }
    if(swkbdInputText(&kb,text,sizeof text)==SWKBD_BUTTON_RIGHT) {
        char *end; unsigned long team=strtoul(text,&end,10);
        if(text[0] && !*end && team>=1 && team<=25599 && set_team((unsigned)team)) {
            snprintf(message,sizeof message,"Team %u saved. Robot disabled.",team_number); save_config();
        } else snprintf(message,sizeof message,"Invalid team number. No changes.");
    }
    memset(&link_state,0,sizeof link_state);
    hidScanInput(); /* Discard keyboard dismiss button edges. */
    safety_disable(&safety);
}
static bool edit_password(void) {
    if(wifi_setup.busy || wifi_scan.scanning || wifi_setup.security!=WIFI_WPA2) return false;
    safety_disable(&safety);
    Controls empty={{0},0,-1}; transmit(&empty,now_ms(),true);
    SwkbdState kb;
    swkbdInit(&kb,SWKBD_TYPE_NORMAL,2,64);
    swkbdSetFeatures(&kb,0);
    swkbdSetHintText(&kb,"Wi-Fi password");
    swkbdSetPasswordMode(&kb,show_password ? SWKBD_PASSWORD_NONE:SWKBD_PASSWORD_HIDE);
    if(wifi_setup.password[0]) swkbdSetInitialText(&kb,wifi_setup.password);
    char value[257]={0};
    bool ok=swkbdInputText(&kb,value,sizeof value)==SWKBD_BUTTON_RIGHT;
    if(ok && strlen(value)<=64) {
        wifi_wipe(wifi_setup.password,sizeof wifi_setup.password);
        memcpy(wifi_setup.password,value,strlen(value)+1);
        wifi_setup.password_saved=false;
    } else ok=false;
    wifi_wipe(value,sizeof value);
    memset(&link_state,0,sizeof link_state); hidScanInput();
    return ok;
}
static bool select_network(unsigned index) {
    if(wifi_setup.busy || wifi_scan.scanning || !wifi_scan.valid ||
       now_ms()-wifi_scan.scanned_at>WIFI_SCAN_FRESH_MS || index>=wifi_scan.count) return false;
    WifiNetwork *n=&wifi_scan.networks[index];
    if(!wifi_security_supported(n->security)) {
        snprintf(wifi_setup.status,sizeof wifi_setup.status,"%s is unsupported on 3DS.",wifi_security_name(n->security));
        return false;
    }
    if(!wifi_setup_select(n->ssid,n->security)) return false;
    show_password=false; popup=POPUP_NONE;
    return n->security==WIFI_OPEN || wifi_setup.password_saved || edit_password();
}
static void join_network(void) {
    if(wifi_setup.busy || wifi_scan.scanning) return;
    bool found=false;
    if(wifi_scan.valid && now_ms()-wifi_scan.scanned_at<=WIFI_SCAN_FRESH_MS)
        for(unsigned i=0;i<wifi_scan.count;i++)
            if(!strcmp(wifi_scan.networks[i].ssid,wifi_setup.ssid) &&
               wifi_scan.networks[i].security==wifi_setup.security && wifi_security_supported(wifi_setup.security)) found=true;
    if(!found) { snprintf(wifi_setup.status,sizeof wifi_setup.status,"Scan and select an available network first."); return; }
    safety_disable(&safety);
    Controls empty={{0},0,-1}; transmit(&empty,now_ms(),true);
    memset(&link_state,0,sizeof link_state);
    bool started=wifi_setup_connect(now_ms());
    /* Profile persistence can succeed even if starting association fails. */
    if(started || wifi_setup.password_saved) save_config();
}
static void draw(const Controls *c, uint64_t now, uint32_t wifi) {
    bool connected=link_connected(&link_state,now);
    int recommended=wifi_scan_recommended(team_number,now);
    UiModel model={
        .connected=connected, .wifi=wifi!=0, .code=link_state.robot.code,
        .enabled=safety.enabled, .rio_enabled=link_state.robot.enabled,
        .estop=safety.estop, .brownout=link_state.robot.brownout,
        .configured=configured, .neutral=neutral(c),
        .voltage=link_state.robot.voltage,
        .age_ms=connected ? (unsigned)(now-link_state.last_rx) : 0,
        .feed=&feed, .alliance=alliance, .team=team_number, .controls=*c, .target=target, .message=message,
        .wifi_ssid=wifi_setup.ssid, .wifi_suggestion=recommended>=0 ? wifi_scan.networks[recommended].ssid:NULL,
        .wifi_password=wifi_setup.password, .wifi_show_password=show_password,
        .networks=wifi_scan.networks, .network_count=wifi_scan.count, .network_page=network_page,
        .wifi_scanning=wifi_scan.scanning, .wifi_status=wifi_scan.scanning ?
            wifi_scan.error ? "Wi-Fi scan timed out; waiting for system.":"Scanning nearby 2.4 GHz networks...":wifi_setup.status,
        .wifi_busy=wifi_setup.busy || wifi_scan.scanning, .wifi_password_set=wifi_setup.password_saved || wifi_setup.password[0],
        .wifi_security=wifi_setup.security, .wifi_available=wifi_setup.available
    };
    ui_draw(&model,page,popup);
}
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    feed_text(&feed,"Waiting for roboRIO console...\n");
    gfxInitDefault();
    if (!display_init()) {
        consoleInit(GFX_TOP, NULL);
        printf("Graphics initialization failed.\nPress START to exit.\n");
        while (aptMainLoop()) { hidScanInput(); if (hidKeysDown() & KEY_START) break; gspWaitForVBlank(); }
        gfxExit(); return 1;
    }
    void *buffer = memalign(0x1000, 0x100000);
    Result result = buffer ? socInit(buffer, 0x100000) : -1;
    bool soc_ready = R_SUCCEEDED(result), ac_ready = false;
    if (soc_ready) {
        ac_ready = R_SUCCEEDED(acInit());
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in local = {0};
        local.sin_family = AF_INET; local.sin_port = htons(DS_STATUS_PORT); local.sin_addr.s_addr = INADDR_ANY;
        if (sock >= 0 && (bind(sock, (struct sockaddr *)&local, sizeof local) != 0 ||
            fcntl(sock, F_SETFL, O_NONBLOCK) != 0)) { close(sock); sock = -1; }
    }
    if (sock < 0 || !ac_ready) {
        display_exit(); consoleInit(GFX_TOP,NULL); printf("Network initialization failed.\nSOC: %08lX errno: %d\nPress START to exit.\n", (unsigned long)result, errno);
        while (aptMainLoop()) { hidScanInput(); if (hidKeysDown() & KEY_START) break; gspWaitForVBlank(); }
    } else {
        wifi_setup_init();
        load_config();
        aptHookCookie hook; aptHook(&hook, lifecycle, NULL);
        uint64_t next_send = 0, next_draw = 0, next_wifi = 0, previous = now_ms();
        uint32_t wifi = 0;
        bool touch_ready=false;
        while (aptMainLoop()) {
            uint64_t now = now_ms();
            if(timing_fault(now,previous)) touch_ready=false;
            previous = now;
            hidScanInput(); uint32_t held = hidKeysHeld(), down = hidKeysDown();
            Controls controls = read_controls(held);
            /* Expire BEFORE receiving, so a recovered link cannot preserve an enable. */
            if (!link_connected(&link_state, now)) {
                if(link_state.have_reply || safety.enabled) touch_ready=false;
                safety_disable(&safety);
                if (link_state.have_reply) memset(&link_state, 0, sizeof link_state);
            }
            if (now >= next_wifi) {
                if (R_FAILED(ACU_GetWifiStatus(&wifi))) wifi = 0;
                next_wifi = now + 200;
            }
            wifi_setup_poll(now);
            bool was_scanning=wifi_scan.scanning;
            wifi_scan_poll(now);
            if(was_scanning && !wifi_scan.scanning && wifi_scan.error)
                snprintf(wifi_setup.status,sizeof wifi_setup.status,"Wi-Fi scan failed (%08lX). Tap Scan to retry.",(unsigned long)wifi_scan.error);
            if(receive_status(now_ms())) touch_ready=false;
            poll_feed(now,configured && wifi && !wifi_setup.busy && !wifi_scan.scanning);
            bool touch_stop=false, touch_enable=false;
            if(!(held & KEY_TOUCH)) touch_ready=true;
            if ((down & KEY_TOUCH) && touch_ready) {
                touch_ready=false;
                touchPosition pos; hidTouchRead(&pos);
                UiAction action=ui_touch(page,popup,pos.px,pos.py,safety.enabled);
                if (action>=UI_TAB_DRIVE && action<=UI_TAB_FEED) {
                    page=action-UI_TAB_DRIVE; show_password=false;
                    if(page==PAGE_WIFI && !wifi_setup.busy && !wifi_scan.scanning) {
                        safety_disable(&safety); wifi_scan_start(now_ms()); network_page=0;
                    }
                }
                else if (action==UI_DISABLE) {
                    safety_disable(&safety);
                    touch_stop=true;
                } else if (action==UI_ENABLE) touch_enable=true;
                else if(action==UI_WIFI_NETWORKS && !wifi_setup.busy) { popup=POPUP_NETWORKS; network_page=0; }
                else if(action==UI_WIFI_SCAN && !wifi_setup.busy && !wifi_scan.scanning) {
                    safety_disable(&safety); wifi_scan_start(now_ms()); network_page=0;
                } else if(action==UI_NETWORK_PREV && network_page) network_page--;
                else if(action==UI_NETWORK_NEXT && (network_page+1)*5<wifi_scan.count) network_page++;
                else if(action>=UI_NETWORK0 && action<=UI_NETWORK4) select_network(network_page*5+action-UI_NETWORK0);
                else if(action==UI_WIFI_PASSWORD) edit_password();
                else if(action==UI_WIFI_SHOW && !wifi_setup.busy && !wifi_scan.scanning) show_password=!show_password;
                else if(action==UI_WIFI_SUGGEST && !wifi_setup.busy) {
                    int index=wifi_scan_recommended(team_number,now_ms());
                    if(index>=0 && select_network(index)) join_network();
                } else if(action==UI_WIFI_CONNECT) {
                    if(wifi_scan.scanning) wifi_scan_cancel();
                    else if(wifi_setup.busy) wifi_setup_cancel(); else join_network();
                }
                else if (action==UI_OPEN_ALLIANCE) popup=POPUP_ALLIANCE;
                else if (action==UI_CLOSE_POPUP) popup=POPUP_NONE;
                else if (action>=UI_RED1 && action<=UI_BLUE3) {
                    alliance=action-UI_RED1; popup=POPUP_NONE; safety_disable(&safety); if(configured) save_config();
                } else if (action==UI_TEAM) edit_team();
            }
            /* Keyboards, service IPC and console parsing can consume this frame.
             * Refresh time before evaluating readiness and before any send. */
            now=now_ms();
            if(timing_fault(now,previous)) {
                touch_ready=false; touch_enable=false;
            }
            SafetyInput input={
                .connected=configured && wifi && !wifi_setup.busy && !wifi_scan.scanning && link_connected(&link_state,now),
                .code=link_state.robot.code, .robot_estop=link_state.robot.estop,
                .enable_pressed=touch_enable, .disable_pressed=touch_stop, .neutral=neutral(&controls)
            };
            safety_step(&safety,&input);
            if(touch_enable && !safety.enabled)
                snprintf(message,sizeof message,"Enable blocked: check link, code and neutral controls.");
            if(touch_enable && safety.enabled) snprintf(message,sizeof message,"Robot enable requested.");
            if(touch_stop) snprintf(message,sizeof message,"Robot disabled from touch screen.");
            if (touch_stop || touch_enable || now >= next_send) {
                transmit(&controls, now_ms(), false); next_send = now_ms() + DS_PERIOD_MS;
            }
            if (now >= next_draw) { draw(&controls, now_ms(), wifi); next_draw = now_ms() + 33; }
            svcSleepThread(2000000); /* ~2ms input/network poll; no blocking DNS. */
        }
        safety_disable(&safety);
        Controls empty = {{0}, 0, -1};
        for (int i = 0; i < 3; ++i) { transmit(&empty, now_ms(), true); svcSleepThread(20000000); }
        aptUnhook(&hook);
    }
    close_feed();
    if (sock >= 0) close(sock);
    if (ac_ready) { wifi_scan_exit(); wifi_setup_exit(); acExit(); }
    if (soc_ready) socExit();
    free(buffer); display_exit(); gfxExit();
    return 0;
}
