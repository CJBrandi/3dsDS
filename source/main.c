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

#define CONFIG_DIR "sdmc:/3ds/3dsDS"
#define CONFIG_PATH CONFIG_DIR "/config.txt"
static PrintConsole top, bottom;
static Safety safety;
static Link link_state;
static struct sockaddr_in robot_addr;
static int sock = -1;
static uint16_t sequence;
static uint8_t mode, alliance;
static char target[16];
static char message[80] = "Tap TEAM or IP to set robot address.";
static bool configured;
static uint32_t lifecycle_event;

static uint64_t now_ms(void) { return svcGetSystemTick() / (SYSCLOCK_ARM11 / 1000); }
static void lifecycle(APT_HookType hook, void *unused) {
    (void)hook; (void)unused;
    __atomic_store_n(&lifecycle_event, 1, __ATOMIC_RELEASE);
}
static bool set_target(const char *ip) {
    struct in_addr address;
    if (inet_pton(AF_INET, ip, &address) != 1) return false;
    uint32_t n = ntohl(address.s_addr);
    if ((n >> 24) == 0 || (n >> 24) == 127 || (n >> 24) >= 224 || (n & 255) == 255) return false;
    memset(&robot_addr, 0, sizeof robot_addr);
    robot_addr.sin_family = AF_INET; robot_addr.sin_port = htons(DS_PORT);
    robot_addr.sin_addr = address;
    snprintf(target, sizeof target, "%s", ip);
    configured = true;
    memset(&link_state, 0, sizeof link_state);
    safety_disable(&safety);
    return true;
}
static void save_config(void) {
    mkdir("sdmc:/3ds", 0777); mkdir(CONFIG_DIR, 0777);
    FILE *f = fopen(CONFIG_PATH ".tmp", "w");
    if (!f) { snprintf(message, sizeof message, "SD save failed; using session settings."); return; }
    bool ok = fprintf(f, "%s %u\n", target, alliance) > 0;
    if (fclose(f) != 0) ok = false;
    if (!ok || rename(CONFIG_PATH ".tmp", CONFIG_PATH) != 0)
        snprintf(message, sizeof message, "SD save failed; using session settings.");
}
static void load_config(void) {
    char ip[16]; unsigned a;
    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) return;
    if (fscanf(f, "%15s %u", ip, &a) == 2 && a <= 5 && set_target(ip)) {
        alliance = a;
        snprintf(message, sizeof message, "Ready. Hold ZL + START to arm.");
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
    const uint32_t keys[7] = {KEY_A, KEY_B, KEY_X, KEY_Y, KEY_L, KEY_R, KEY_ZR};
    for (int i = 0; i < 7; ++i) if (held & keys[i]) c.buttons |= 1u << i;
    c.pov = ds_pov(held & KEY_DUP, held & KEY_DRIGHT, held & KEY_DDOWN, held & KEY_DLEFT);
    return c;
}
static bool neutral(const Controls *c) {
    return !c->axes[0] && !c->axes[1] && !c->axes[2] && !c->axes[3] && !c->buttons && c->pov == -1;
}
static void edit_address(bool team) {
    Controls empty = {{0}, 0, -1};
    safety_disable(&safety);
    transmit(&empty, now_ms(), true);
    SwkbdState kb;
    swkbdInit(&kb, team ? SWKBD_TYPE_NUMPAD : SWKBD_TYPE_NORMAL, 2, team ? 5 : 15);
    swkbdSetHintText(&kb, team ? "FRC team number (1-25599)" : "Robot IPv4, e.g. 10.12.34.2");
    if (!team && configured) swkbdSetInitialText(&kb, target);
    char text[32] = {0}, ip[16] = {0};
    if (swkbdInputText(&kb, text, sizeof text) == SWKBD_BUTTON_RIGHT) {
        bool valid = true;
        if (team) {
            char *end; unsigned long value = strtoul(text, &end, 10);
            valid = text[0] && !*end && value >= 1 && value <= 25599;
            if (valid) snprintf(ip, sizeof ip, "10.%lu.%lu.2", value / 100, value % 100);
        } else snprintf(ip, sizeof ip, "%.15s", text);
        if (valid && set_target(ip)) {
            snprintf(message, sizeof message, "Address saved. Robot disabled."); save_config();
        } else snprintf(message, sizeof message, "Invalid address/team. No changes.");
    }
    memset(&link_state, 0, sizeof link_state);
    hidScanInput(); /* Discard keyboard dismiss button edges. */
    safety_disable(&safety);
}
static void draw(const Controls *c, uint64_t now, uint32_t wifi) {
    bool connected = link_connected(&link_state, now);
    const char *modes[] = {"TELEOP", "TEST", "AUTO"};
    consoleSelect(&top); printf("\x1b[H\x1b[2J");
    printf("\x1b[36m  3dsDS / roboRIO\x1b[0m\n\n");
    printf("  %s\n\n", safety.estop ? "\x1b[31mE-STOP LATCHED\x1b[0m" :
           safety.enabled ? "\x1b[32mENABLE REQUESTED\x1b[0m" : "\x1b[33mDISABLED\x1b[0m");
    printf("  Robot  %s\n", configured ? target : "not configured");
    printf("  Link   %s   Wi-Fi %s\n", connected ? "CONNECTED" : "LOST", wifi ? "ON" : "OFF");
    printf("  Code   %s\n", connected && link_state.robot.code ? "RUNNING" : "NOT READY");
    if (connected) {
        printf("  Battery %.2f V %s\n", link_state.robot.voltage, link_state.robot.brownout ? "BROWNOUT" : "");
        printf("  RIO     %s  Reply age %llu ms\n", link_state.robot.enabled ? "ENABLED" : "DISABLED",
               (unsigned long long)(now - link_state.last_rx));
    } else printf("  Battery --     RIO state unknown\n\n");
    printf("\n  Mode %s  Station %s%u\n", modes[mode], alliance < 3 ? "RED " : "BLUE ", alliance % 3 + 1);
    printf("\n  Circle %4d %4d\n  C-stick%4d %4d\n", c->axes[0], c->axes[1], c->axes[2], c->axes[3]);
    printf("  Buttons %02X   POV %d\n", c->buttons, c->pov);
    printf("\n  %s\n", safety.arming ? "Hold START..." : "ZL is the hold-to-run switch.");
    printf("\n  %.46s\n", message);
    consoleSelect(&bottom); printf("\x1b[H\x1b[2J");
    printf("  \x1b[36mDRIVER CONTROLS\x1b[0m\n\n");
    printf("  Hold ZL + START 1s: enable\n  Release ZL: disable\n  SELECT / START: disable\n  L + R + SELECT: E-STOP\n\n");
    printf("  Release START after arming.\n  Keep ZL held while driving.\n");
    printf("\n  [ MODE: %-6s ]\n", modes[mode]); /* Row 10. */
    printf("\n  [ STATION: %s%u ]\n", alliance < 3 ? "RED " : "BLUE ", alliance % 3 + 1); /* Row 12. */
    printf("\n  [ TEAM NUMBER ]\n\n  [ ROBOT IP ADDRESS ]\n\n  [ QUIT ]\n");
    printf("\n  Tap settings while disabled.\n  Joystick 0 / built-in controls\n");
    if (safety.estop) printf("\n  Reset E-stop at robot; restart app.\n");
    gfxFlushBuffers(); gfxSwapBuffers();
}
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    gfxInitDefault(); consoleInit(GFX_TOP, &top); consoleInit(GFX_BOTTOM, &bottom);
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
        consoleSelect(&top); printf("Network initialization failed.\nSOC: %08lX errno: %d\nPress START to exit.\n", (unsigned long)result, errno);
        while (aptMainLoop()) { hidScanInput(); if (hidKeysDown() & KEY_START) break; gspWaitForVBlank(); }
    } else {
        load_config();
        aptHookCookie hook; aptHook(&hook, lifecycle, NULL);
        uint64_t next_send = 0, next_draw = 0, next_wifi = 0, previous = now_ms();
        uint32_t wifi = 0;
        bool quit = false;
        while (!quit && aptMainLoop()) {
            uint64_t now = now_ms();
            if (__atomic_exchange_n(&lifecycle_event, 0, __ATOMIC_ACQ_REL) || now - previous > 100) {
                safety_disable(&safety); memset(&link_state, 0, sizeof link_state);
            }
            previous = now;
            hidScanInput(); uint32_t held = hidKeysHeld(), down = hidKeysDown();
            Controls controls = read_controls(held);
            /* Expire BEFORE receiving, so a recovered link cannot preserve an enable. */
            if (!link_connected(&link_state, now)) {
                safety_disable(&safety);
                if (link_state.have_reply) memset(&link_state, 0, sizeof link_state);
            }
            if (now >= next_wifi) {
                if (R_FAILED(ACU_GetWifiStatus(&wifi))) wifi = 0;
                next_wifi = now + 200;
            }
            receive_status(now);
            SafetyInput input = {
                configured && wifi && link_connected(&link_state, now), link_state.robot.code,
                link_state.robot.estop, (held & KEY_ZL) != 0, (held & KEY_START) != 0,
                (held & KEY_SELECT) != 0,
                (held & (KEY_L | KEY_R | KEY_SELECT)) == (KEY_L | KEY_R | KEY_SELECT), neutral(&controls)
            };
            safety_step(&safety, &input, now);
            if ((down & KEY_TOUCH) && !safety.enabled) {
                touchPosition pos; hidTouchRead(&pos);
                int row = pos.py / 8;
                if (pos.px >= 8 && pos.px < 310) {
                    if (row == 10) { mode = (mode + 1) % 3; safety_disable(&safety); }
                    else if (row == 12) { alliance = (alliance + 1) % 6; safety_disable(&safety); if (configured) save_config(); }
                    else if (row == 14 || row == 16) edit_address(row == 14);
                    else if (row == 18) quit = true;
                }
            }
            if (quit || now >= next_send || (!safety.enabled && (down & (KEY_SELECT | KEY_START)))) {
                transmit(&controls, now_ms(), quit); next_send = now_ms() + DS_PERIOD_MS;
            }
            if (now >= next_draw) { draw(&controls, now_ms(), wifi); next_draw = now_ms() + 100; }
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
    free(buffer); gfxExit();
    return 0;
}
