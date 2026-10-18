#ifndef N3DS_WIFI_SCAN_H
#define N3DS_WIFI_SCAN_H
#include "wifi_settings.h"
#define WIFI_MAX_NETWORKS 24
#define WIFI_SCAN_FRESH_MS 30000
#define WIFI_SCAN_TIMEOUT_MS 10000

typedef struct { char ssid[33]; uint8_t bssid[6],channel; WifiSecurity security; } WifiNetwork;
bool wifi_parse_beacons(const uint8_t *data,size_t length,WifiNetwork *networks,size_t capacity,size_t *count);
int wifi_team_score(const char *ssid,unsigned team);
const char *wifi_security_name(WifiSecurity security);
bool wifi_security_supported(WifiSecurity security);
/* Native scanner owns its worker and uses only actual infrastructure beacons. */
typedef struct {
    WifiNetwork networks[WIFI_MAX_NETWORKS];
    size_t count;
    bool scanning,valid;
    uint64_t scanned_at;
    uint32_t error;
} WifiScan;
extern WifiScan wifi_scan;
bool wifi_scan_start(uint64_t now);
void wifi_scan_poll(uint64_t now);
void wifi_scan_cancel(void);
void wifi_scan_exit(void);
int wifi_scan_recommended(unsigned team,uint64_t now);
#endif
