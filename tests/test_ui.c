#include <assert.h>
#include <stdio.h>
#include "ui.h"
/* No drawing is needed to exercise the touch authorization boundary. */
void paint_begin(int s) {(void)s;}
void paint_end(void) {}
void paint_box(float x,float y,float w,float h,uint32_t c) {(void)x;(void)y;(void)w;(void)h;(void)c;}
void paint_text(float x,float y,float size,uint32_t c,const char *s) {(void)x;(void)y;(void)size;(void)c;(void)s;}
int main(void) {
    assert(ui_touch(PAGE_DRIVE,20,65,false)==UI_TELEOP);
    assert(ui_touch(PAGE_DRIVE,120,65,false)==UI_TEST);
    assert(ui_touch(PAGE_DRIVE,230,65,false)==UI_AUTO);
    for(int x=0;x<320;x++) for(int y=0;y<240;y++) {
        UiAction a=ui_touch(PAGE_DRIVE,x,y,true);
        assert(a==UI_NONE || a==UI_DISABLE || a==UI_ESTOP ||
               (a>=UI_TAB_DRIVE && a<=UI_TAB_INPUTS));
        a=ui_touch(PAGE_SETUP,x,y,true);
        assert(a==UI_NONE || (a>=UI_TAB_DRIVE && a<=UI_TAB_INPUTS));
    }
    assert(ui_touch(PAGE_DRIVE,20,155,true)==UI_DISABLE);
    assert(ui_touch(PAGE_DRIVE,230,155,true)==UI_ESTOP);
    assert(ui_touch(PAGE_SETUP,50,85,false)==UI_IP);
    assert(ui_touch(PAGE_SETUP,50,135,false)==UI_TEAM);
    assert(ui_touch(PAGE_SETUP,50,210,false)==UI_QUIT);
    assert(ui_touch(PAGE_DRIVE,-1,65,false)==UI_NONE);
    assert(ui_touch(PAGE_DRIVE,320,65,false)==UI_NONE);
    puts("PASS: enabled UI blocks configuration and preserves stop controls");
}
