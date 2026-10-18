#ifndef N3DS_WIFI_H
#define N3DS_WIFI_H
#include "wifi_settings.h"
/* Password stays in RAM for explicit Show/Edit; persisted only in the system profile. */
typedef struct {
    char ssid[33], password[65], profiles[3][33], status[96];
    WifiSecurity security;
    unsigned slot;
    bool busy, password_saved, available;
    uint32_t error;
} WifiSetup;
extern WifiSetup wifi_setup;
void wifi_setup_init(void);
void wifi_setup_refresh_profiles(void);
bool wifi_setup_connect(uint64_t now);
void wifi_setup_poll(uint64_t now);
void wifi_setup_cancel(void);
void wifi_setup_exit(void);
#endif
