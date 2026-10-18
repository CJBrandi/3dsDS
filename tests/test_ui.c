#include <assert.h>
#include <stdio.h>
#include "ui.h"
void paint_begin(int s) {(void)s;}
void paint_end(void) {}
void paint_text_centered(float x,float y,float size,uint32_t c,const char *s) {(void)x;(void)y;(void)size;(void)c;(void)s;}
void paint_box(float x,float y,float w,float h,uint32_t c) {(void)x;(void)y;(void)w;(void)h;(void)c;}
void paint_text(float x,float y,float size,uint32_t c,const char *s) {(void)x;(void)y;(void)size;(void)c;(void)s;}
int main(void) {
    /* Every valid pixel stops, on every page, even with an old popup open. */
    for(int page=0;page<3;page++) for(int popup=0;popup<3;popup++)
        for(int x=0;x<320;x++) for(int y=0;y<240;y++)
            assert(ui_touch(page,popup,x,y,true)==UI_DISABLE);
    assert(ui_touch(PAGE_DRIVE,POPUP_NONE,20,65,false)==UI_OPEN_ALLIANCE);
    assert(ui_touch(PAGE_DRIVE,POPUP_NONE,20,125,false)==UI_TEAM);
    assert(ui_touch(PAGE_DRIVE,POPUP_NONE,20,180,false)==UI_ENABLE);
    assert(ui_touch(PAGE_DRIVE,POPUP_ALLIANCE,20,200,false)==UI_BLUE3);
    /* Dismissing a selector must not enable the robot underneath it. */
    assert(ui_touch(PAGE_DRIVE,POPUP_ALLIANCE,20,220,false)==UI_CLOSE_POPUP);
    assert(ui_touch(PAGE_DRIVE,POPUP_NONE,50,105,false)==UI_NONE);
    assert(ui_touch(PAGE_DRIVE,POPUP_NONE,50,20,false)==UI_TAB_DRIVE);
    assert(ui_touch(PAGE_DRIVE,POPUP_NONE,150,20,false)==UI_TAB_INPUTS);
    assert(ui_touch(PAGE_DRIVE,POPUP_NONE,250,20,false)==UI_TAB_WIFI);
    assert(ui_touch(PAGE_DRIVE,POPUP_NONE,-1,65,true)==UI_NONE);
    assert(ui_touch(PAGE_DRIVE,POPUP_NONE,320,65,true)==UI_NONE);
    assert(ui_touch(PAGE_WIFI,POPUP_NONE,50,100,false)==UI_WIFI_NETWORKS);
    assert(ui_touch(PAGE_WIFI,POPUP_NONE,50,140,false)==UI_WIFI_PASSWORD);
    assert(ui_touch(PAGE_WIFI,POPUP_NONE,275,140,false)==UI_WIFI_SHOW);
    assert(ui_touch(PAGE_WIFI,POPUP_NONE,275,100,false)==UI_WIFI_SCAN);
    assert(ui_touch(PAGE_WIFI,POPUP_NONE,275,60,false)==UI_WIFI_SUGGEST);
    assert(ui_touch(PAGE_WIFI,POPUP_NONE,50,210,false)==UI_WIFI_CONNECT);
    assert(ui_touch(PAGE_WIFI,POPUP_NETWORKS,50,100,false)==UI_NETWORK1);
    assert(ui_touch(PAGE_WIFI,POPUP_NETWORKS,250,220,false)==UI_NETWORK_NEXT);
    assert(ui_touch(PAGE_WIFI,POPUP_NETWORKS,160,220,false)==UI_CLOSE_POPUP);
    puts("PASS: full-screen stop target, dropdown isolation, merged numeric team field and scanned network picker");
}
