#ifndef N3DS_WIFI_SETTINGS_H
#define N3DS_WIFI_SETTINGS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum { WIFI_WPA2, WIFI_OPEN, WIFI_UNSUPPORTED, WIFI_WPA3, WIFI_ENTERPRISE } WifiSecurity;
bool wifi_credentials_valid(const char *ssid, const char *password, WifiSecurity mode);
/* All passwords are cleared through a volatile write. */
void wifi_wipe(void *buffer,size_t length);
#endif
