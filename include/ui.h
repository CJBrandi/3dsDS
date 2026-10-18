#ifndef N3DS_UI_H
#define N3DS_UI_H
#include "protocol.h"
#include "feed.h"
#include "wifi_scan.h"
typedef enum { PAGE_DRIVE, PAGE_INPUTS, PAGE_WIFI, PAGE_FEED } UiPage;
typedef enum { POPUP_NONE, POPUP_ALLIANCE, POPUP_NETWORKS } UiPopup;
typedef enum {
    UI_NONE, UI_TAB_DRIVE, UI_TAB_INPUTS, UI_TAB_WIFI, UI_TAB_FEED,
    UI_RED1, UI_RED2, UI_RED3,
    UI_BLUE1, UI_BLUE2, UI_BLUE3, UI_ENABLE, UI_DISABLE, UI_TEAM, UI_WIFI_NETWORKS, UI_WIFI_PASSWORD, UI_WIFI_SUGGEST, UI_WIFI_CONNECT, UI_WIFI_SCAN, UI_WIFI_SHOW, UI_NETWORK_PREV, UI_NETWORK_NEXT, UI_NETWORK0, UI_NETWORK1, UI_NETWORK2, UI_NETWORK3, UI_NETWORK4, UI_OPEN_ALLIANCE, UI_CLOSE_POPUP
} UiAction;
typedef struct {
    bool connected, wifi, code, enabled, rio_enabled, estop, brownout, configured, neutral;
    float voltage;
    unsigned age_ms;
    uint8_t alliance;
    unsigned team;
    Controls controls;
    const Feed *feed;
    const char *target, *message;
    const char *wifi_ssid, *wifi_suggestion, *wifi_password, *wifi_status;
    const WifiNetwork *networks;
    unsigned network_count, network_page;
    bool wifi_show_password, wifi_scanning;
    bool wifi_busy, wifi_password_set, wifi_available;
    WifiSecurity wifi_security;

} UiModel;
/* Backend coordinates are native screen pixels, colors are 0xRRGGBB. */
void paint_begin(int screen);
void paint_box(float x, float y, float w, float h, uint32_t color);
void paint_text(float x, float y, float size, uint32_t color, const char *text);
void paint_text_centered(float cx,float cy,float size,uint32_t color,const char *text);
void paint_feed_line(float x,float y,float width,float size,uint32_t color,const char *text);
void paint_end(void);
void ui_draw(const UiModel *m, UiPage page, UiPopup popup);
UiAction ui_touch(UiPage page, UiPopup popup, int x, int y, bool enabled);
bool display_init(void);
void display_exit(void);
#endif
