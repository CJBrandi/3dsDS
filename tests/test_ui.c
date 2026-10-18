#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"
static char drive_reason[80];
void paint_begin(int s) {(void)s;}
void paint_end(void) {}
void paint_feed_line(float x,float y,float w,float size,uint32_t c,const char *s) {(void)x;(void)y;(void)w;(void)size;(void)c;(void)s;}
void paint_text_centered(float x,float y,float size,uint32_t c,const char *s) {(void)x;(void)y;(void)size;(void)c;(void)s;}
void paint_box(float x,float y,float w,float h,uint32_t c) {(void)x;(void)y;(void)w;(void)h;(void)c;}
void paint_text(float x,float y,float size,uint32_t c,const char *s) {
    (void)x;(void)size;(void)c;
    if(y==219) snprintf(drive_reason,sizeof drive_reason,"%s",s);
    for(const unsigned char *p=(const unsigned char *)s;*p;p++) assert(*p>=32 && *p<=126);
}
int main(void) {
    /* Every valid pixel stops, on every page, even with an old popup open. */
    for(int page=0;page<4;page++) for(int popup=0;popup<3;popup++)
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
    assert(ui_touch(PAGE_DRIVE,POPUP_NONE,110,20,false)==UI_TAB_INPUTS);
    assert(ui_touch(PAGE_DRIVE,POPUP_NONE,190,20,false)==UI_TAB_WIFI);
    assert(ui_touch(PAGE_DRIVE,POPUP_NONE,280,20,false)==UI_TAB_FEED);
    for(int x=0;x<320;x++) for(int y=42;y<240;y++)
        assert(ui_touch(PAGE_FEED,POPUP_NONE,x,y,false)==UI_NONE);
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
    UiModel model={.configured=true,.connected=true,.code=true,.neutral=true,.target="10.12.34.2",.message=""};
    ui_draw(&model,PAGE_DRIVE,POPUP_NONE);
    assert(!strcmp(drive_reason,"Connect to Wi-Fi first"));
    model.wifi=true;model.wifi_busy=true;
    ui_draw(&model,PAGE_DRIVE,POPUP_NONE);
    assert(!strcmp(drive_reason,"Wait for Wi-Fi operation to finish"));
    WifiNetwork network={.ssid="FRC-\xff",.security=WIFI_OPEN};
    model.wifi_ssid=model.wifi_suggestion=network.ssid;
    model.networks=&network;model.network_count=1;
    ui_draw(&model,PAGE_WIFI,POPUP_NETWORKS); /* Raw non-UTF-8 SSIDs never reach the font parser. */
    puts("PASS: full-screen stop target, dropdown isolation, merged numeric team field and scanned network picker");
}
