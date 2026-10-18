#ifndef N3DS_UI_H
#define N3DS_UI_H
#include "protocol.h"
#include "wifi_settings.h"
typedef enum { PAGE_DRIVE, PAGE_SETUP, PAGE_INPUTS, PAGE_WIFI } UiPage;
typedef enum { POPUP_NONE, POPUP_MODE, POPUP_ALLIANCE, POPUP_SECURITY, POPUP_WIFI_SLOT } UiPopup;
typedef enum {
    UI_NONE, UI_TAB_DRIVE, UI_TAB_SETUP, UI_TAB_INPUTS, UI_TAB_WIFI,
    UI_TELEOP, UI_TEST, UI_AUTO, UI_RED1, UI_RED2, UI_RED3,
    UI_BLUE1, UI_BLUE2, UI_BLUE3, UI_ENABLE, UI_DISABLE, UI_TEAM, UI_WIFI_SSID, UI_WIFI_PASSWORD, UI_WIFI_SUGGEST, UI_WIFI_CONNECT, UI_OPEN_SECURITY, UI_OPEN_WIFI_SLOT, UI_SECURITY_WPA2, UI_SECURITY_OPEN, UI_SLOT1, UI_SLOT2, UI_SLOT3, UI_OPEN_MODE, UI_OPEN_ALLIANCE, UI_CLOSE_POPUP
} UiAction;
typedef struct {
    bool connected, wifi, code, enabled, rio_enabled, estop, brownout, configured, neutral;
    float voltage;
    unsigned age_ms;
    uint8_t mode, alliance;
    unsigned team;
    Controls controls;
    const char *target, *message;
    const char *wifi_ssid, *wifi_suggestion, *wifi_slot_ssid, *wifi_status;
    bool wifi_busy, wifi_password_set, wifi_available;
    WifiSecurity wifi_security;
    unsigned wifi_slot;
} UiModel;
/* Backend coordinates are native screen pixels, colors are 0xRRGGBB. */
void paint_begin(int screen);
void paint_box(float x, float y, float w, float h, uint32_t color);
void paint_text(float x, float y, float size, uint32_t color, const char *text);
void paint_text_centered(float cx,float cy,float size,uint32_t color,const char *text);
void paint_end(void);
void ui_draw(const UiModel *m, UiPage page, UiPopup popup);
UiAction ui_touch(UiPage page, UiPopup popup, int x, int y, bool enabled);
bool display_init(void);
void display_exit(void);
#endif
