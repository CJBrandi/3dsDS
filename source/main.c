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
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "protocol.h"
#include "safety.h"
#include "link.h"
#include "ui.h"

#define CONFIG_DIR "sdmc:/3ds/3dsDS"
#define CONFIG_PATH CONFIG_DIR "/config.txt"
static UiPage page;
static UiPopup popup;
static Safety safety;
static Link link_state;
static struct sockaddr_in robot_addr;
static int sock = -1;
static uint16_t sequence;
static uint8_t mode, alliance;
static unsigned team_number;
static char target[16];
static char message[80] = "Set your team number in Setup.";
static bool configured;
static uint32_t lifecycle_event;

static uint64_t now_ms(void) { return svcGetSystemTick() / (SYSCLOCK_ARM11 / 1000); }
static bool transmit(const Controls *controls, uint64_t now, bool force_disabled);
static void lifecycle(APT_HookType hook, void *unused) {
    (void)unused;
    /* libctru invokes these hooks synchronously before entering HOME/sleep. */
    safety_disable(&safety);
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
    team_number=team; configured=true;
    memset(&link_state,0,sizeof link_state); safety_disable(&safety);
    return true;
}
static void save_config(void) {
    mkdir("sdmc:/3ds", 0777); mkdir(CONFIG_DIR, 0777);
    FILE *f = fopen(CONFIG_PATH ".tmp", "w");
    if (!f) { snprintf(message, sizeof message, "SD save failed; using session settings."); return; }
    bool ok = fprintf(f, "%u %u\n", team_number, alliance) > 0;
    if (fclose(f) != 0) ok = false;
    if (!ok || rename(CONFIG_PATH ".tmp", CONFIG_PATH) != 0)
        snprintf(message, sizeof message, "SD save failed; using session settings.");
}
static void load_config(void) {
    unsigned team,a;
    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) return;
    if (fscanf(f, "%u %u", &team, &a) == 2 && a <= 5 && set_team(team)) {
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
    uint8_t data[DS_PACKET_MAX];
    Command cmd = {sequence++, mode, safety.enabled && !force_disabled, safety.estop, alliance};
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
static void receive_status(uint64_t now) {
    /* Bounded work per frame, including on a noisy network. */
    for (int count = 0; count < 32; ++count) {
        uint8_t data[2048]; struct sockaddr_in peer; socklen_t len = sizeof peer;
        ssize_t n = recvfrom(sock, data, sizeof data, 0, (struct sockaddr *)&peer, &len);
        if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                safety_disable(&safety); memset(&link_state, 0, sizeof link_state);
                snprintf(message, sizeof message, "Receive failed (%d).", errno);
            }
            break;
        }
        if (configured && peer.sin_family == AF_INET &&
            peer.sin_addr.s_addr == robot_addr.sin_addr.s_addr)
            link_receive(&link_state, data, (size_t)n, now);
    }
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
static void draw(const Controls *c, uint64_t now, uint32_t wifi) {
    bool connected=link_connected(&link_state,now);
    UiModel model={
        .connected=connected, .wifi=wifi!=0, .code=link_state.robot.code,
        .enabled=safety.enabled, .rio_enabled=link_state.robot.enabled,
        .estop=safety.estop, .brownout=link_state.robot.brownout,
        .configured=configured, .neutral=neutral(c),
        .voltage=link_state.robot.voltage,
        .age_ms=connected ? (unsigned)(now-link_state.last_rx) : 0,
        .mode=mode, .alliance=alliance, .team=team_number, .controls=*c, .target=target, .message=message
    };
    ui_draw(&model,page,popup);
}
int main(int argc, char **argv) {
    (void)argc; (void)argv;
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
        load_config();
        aptHookCookie hook; aptHook(&hook, lifecycle, NULL);
        uint64_t next_send = 0, next_draw = 0, next_wifi = 0, previous = now_ms();
        uint32_t wifi = 0;
        bool quit = false, touch_ready=false;
        while (!quit && aptMainLoop()) {
            uint64_t now = now_ms();
            if (__atomic_exchange_n(&lifecycle_event, 0, __ATOMIC_ACQ_REL) || now - previous > 100) {
                safety_disable(&safety); memset(&link_state, 0, sizeof link_state);
                touch_ready=false;
            }
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
            receive_status(now);
            bool touch_stop=false, touch_enable=false;
            if(!(held & KEY_TOUCH)) touch_ready=true;
            if ((down & KEY_TOUCH) && touch_ready) {
                touch_ready=false;
                touchPosition pos; hidTouchRead(&pos);
                UiAction action=ui_touch(page,popup,pos.px,pos.py,safety.enabled);
                if (action>=UI_TAB_DRIVE && action<=UI_TAB_INPUTS) page=action-UI_TAB_DRIVE;
                else if (action==UI_DISABLE) {
                    safety_disable(&safety);
                    touch_stop=true;
                } else if (action==UI_ENABLE) touch_enable=true;
                else if (action==UI_OPEN_MODE) popup=POPUP_MODE;
                else if (action==UI_OPEN_ALLIANCE) popup=POPUP_ALLIANCE;
                else if (action==UI_CLOSE_POPUP) popup=POPUP_NONE;
                else if (action>=UI_TELEOP && action<=UI_AUTO) {
                    mode=action-UI_TELEOP; popup=POPUP_NONE; safety_disable(&safety);
                } else if (action>=UI_RED1 && action<=UI_BLUE3) {
                    alliance=action-UI_RED1; popup=POPUP_NONE; safety_disable(&safety); if(configured) save_config();
                } else if (action==UI_TEAM) edit_team();
                else if (action==UI_QUIT) quit=true;
            }
            SafetyInput input={
                .connected=configured && wifi && link_connected(&link_state,now),
                .code=link_state.robot.code, .robot_estop=link_state.robot.estop,
                .enable_pressed=touch_enable, .disable_pressed=touch_stop, .neutral=neutral(&controls)
            };
            safety_step(&safety,&input);
            if(touch_enable && !safety.enabled)
                snprintf(message,sizeof message,"Enable blocked: check link, code and neutral controls.");
            if(touch_enable && safety.enabled) snprintf(message,sizeof message,"Robot enable requested.");
            if(touch_stop) snprintf(message,sizeof message,"Robot disabled from touch screen.");
            if (quit || touch_stop || touch_enable || now >= next_send) {
                transmit(&controls, now_ms(), quit); next_send = now_ms() + DS_PERIOD_MS;
            }
            if (now >= next_draw) { draw(&controls, now_ms(), wifi); next_draw = now_ms() + 33; }
            svcSleepThread(2000000); /* ~2ms input/network poll; no blocking DNS. */
        }
        safety_disable(&safety);
        Controls empty = {{0}, 0, -1};
        for (int i = 0; i < 3; ++i) { transmit(&empty, now_ms(), true); svcSleepThread(20000000); }
        aptUnhook(&hook);
    }
    if (sock >= 0) close(sock);
    if (ac_ready) acExit();
    if (soc_ready) socExit();
    free(buffer); display_exit(); gfxExit();
    return 0;
}
