#include <assert.h>
#include <stdio.h>
#include "ui.h"
void paint_begin(int s) {(void)s;}
void paint_end(void) {}
void paint_box(float x,float y,float w,float h,uint32_t c) {(void)x;(void)y;(void)w;(void)h;(void)c;}
void paint_text(float x,float y,float size,uint32_t c,const char *s) {(void)x;(void)y;(void)size;(void)c;(void)s;}
int main(void) {
    /* Every valid pixel stops, on every page, even with an old popup open. */
    for(int page=0;page<3;page++) for(int popup=0;popup<3;popup++)
        for(int x=0;x<320;x++) for(int y=0;y<240;y++)
            assert(ui_touch(page,popup,x,y,true)==UI_DISABLE);
    assert(ui_touch(PAGE_DRIVE,POPUP_NONE,20,65,false)==UI_OPEN_MODE);
    assert(ui_touch(PAGE_DRIVE,POPUP_NONE,20,125,false)==UI_OPEN_ALLIANCE);
    assert(ui_touch(PAGE_DRIVE,POPUP_NONE,20,180,false)==UI_ENABLE);
    assert(ui_touch(PAGE_DRIVE,POPUP_MODE,20,120,false)==UI_TEST);
    assert(ui_touch(PAGE_DRIVE,POPUP_MODE,20,170,false)==UI_AUTO);
    assert(ui_touch(PAGE_DRIVE,POPUP_ALLIANCE,20,200,false)==UI_BLUE3);
    /* A dismissed dropdown must not activate the Enable button underneath it. */
    assert(ui_touch(PAGE_DRIVE,POPUP_MODE,20,220,false)==UI_CLOSE_POPUP);
    assert(ui_touch(PAGE_SETUP,POPUP_NONE,50,85,false)==UI_TEAM);
    assert(ui_touch(PAGE_SETUP,POPUP_NONE,50,135,false)==UI_NONE);
    assert(ui_touch(PAGE_SETUP,POPUP_NONE,50,210,false)==UI_QUIT);
    assert(ui_touch(PAGE_DRIVE,POPUP_NONE,-1,65,true)==UI_NONE);
    assert(ui_touch(PAGE_DRIVE,POPUP_NONE,320,65,true)==UI_NONE);
    puts("PASS: full-screen stop target, dropdown isolation, team-only setup");
}
