#ifndef N3DS_UI_H
#define N3DS_UI_H
#include "protocol.h"
typedef enum { PAGE_DRIVE, PAGE_SETUP, PAGE_INPUTS } UiPage;
typedef enum {
    UI_NONE, UI_TAB_DRIVE, UI_TAB_SETUP, UI_TAB_INPUTS,
    UI_TELEOP, UI_TEST, UI_AUTO, UI_RED1, UI_RED2, UI_RED3,
    UI_BLUE1, UI_BLUE2, UI_BLUE3, UI_DISABLE, UI_ESTOP, UI_TEAM, UI_IP, UI_QUIT
} UiAction;
typedef struct {
    bool connected, wifi, code, enabled, rio_enabled, estop, brownout, arming, configured, deadman;
    float voltage, arm_progress;
    unsigned age_ms;
    uint8_t mode, alliance;
    Controls controls;
    const char *target, *message;
} UiModel;
/* Backend coordinates are native screen pixels, colors are 0xRRGGBB. */
void paint_begin(int screen);
void paint_box(float x, float y, float w, float h, uint32_t color);
void paint_text(float x, float y, float size, uint32_t color, const char *text);
void paint_end(void);
void ui_draw(const UiModel *m, UiPage page);
UiAction ui_touch(UiPage page, int x, int y, bool enabled);
bool display_init(void);
void display_exit(void);
#endif
