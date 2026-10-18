#include "ui.h"
#include <stdio.h>
#define BG 0x0c131d
#define PANEL 0x162332
#define LINE 0x25384b
#define WHITE 0xecf4fa
#define MUTED 0x94aabd
#define TEAL 0x49dfbd
#define AMBER 0xffcf75
#define RED 0xff6b78
static const char *modes[] = {"TELEOP", "TEST", "AUTO"};
static void text(float x, float y, float size, uint32_t color, const char *s) {
    paint_text(x, y, size, color, s);
}
static void box(float x, float y, float w, float h, uint32_t color) { paint_box(x,y,w,h,color); }
static void button(int x, int y, int w, int h, const char *label, bool selected, bool locked) {
    box(x,y,w,h,selected && !locked ? 0x174d48 : PANEL);
    box(x,y+h-2,w,2,selected && !locked ? TEAL : LINE);
    text(x+8,y+(h-14)/2,13,locked ? 0x526579 : selected ? TEAL : WHITE,label);
}
static void metric(int x, const char *name, const char *value, uint32_t color) {
    box(x,120,120,44,PANEL); text(x+10,125,9,MUTED,name); text(x+10,139,16,color,value);
}
static void stick(int x, int y, const char *name, int8_t a, int8_t b) {
    text(x,y,10,MUTED,name);
    box(x,y+18,96,70,PANEL);
    box(x+47,y+18,1,70,LINE); box(x,y+52,96,1,LINE);
    float px = x+48+a*0.31f, py = y+53+b*0.22f;
    box(px-4,py-4,8,8,TEAL);
}
void ui_draw(const UiModel *m, UiPage page, UiPopup popup) {
    char s[96];
    paint_begin(0); box(0,0,400,240,BG);
    text(14,10,19,WHITE,"3dsDS"); text(93,15,10,MUTED,"ROBOT CONTROL");
    box(289,10,97,23,m->connected ? 0x174d48 : PANEL);
    text(299,16,10,m->connected ? TEAL : MUTED,m->connected ? "CONNECTED" : "OFFLINE");
    box(14,45,372,65,PANEL);
    uint32_t state = m->estop ? RED : m->enabled ? TEAL : AMBER;
    box(14,45,4,65,state);
    text(28,53,10,MUTED,"DRIVER STATION");
    text(28,69,m->enabled ? 19 : 25,state,m->estop ? "E-STOP" : m->enabled ? "ENABLE REQUESTED" : "DISABLED");
    text(290,53,10,MUTED,"BATTERY");
    if (m->connected) snprintf(s,sizeof s,"%.2f V",m->voltage); else snprintf(s,sizeof s,"--.-- V");
    text(290,71,20,m->brownout ? RED : WHITE,s);
    metric(14,"NETWORK",m->wifi ? "Wi-Fi on" : "Wi-Fi off",m->wifi ? TEAL : MUTED);
    metric(140,"ROBOT CODE",m->connected && m->code ? "Running" : "Not ready",m->connected && m->code ? TEAL : AMBER);
    if (m->connected) snprintf(s,sizeof s,"%u ms",m->age_ms); else snprintf(s,sizeof s,"No reply");
    metric(266,"REPLY AGE",s,m->connected ? WHITE : MUTED);
    snprintf(s,sizeof s,"%s  /  %s %u",modes[m->mode],m->alliance < 3 ? "RED" : "BLUE",m->alliance%3+1);
    text(14,176,12,WHITE,s);
    text(14,195,11,MUTED,m->configured ? m->target : "Set your team number in Setup");
    text(246,176,10,MUTED,"ROBOT REPORTS");
    text(246,193,12,m->connected && m->rio_enabled ? TEAL : MUTED,
         !m->connected ? "Unknown" : m->rio_enabled ? "Enabled" : "Disabled");
    box(0,221,400,19,0x111e2b);
    snprintf(s,sizeof s,"%.65s",m->message);
    text(12,225,9,MUTED,s);
    paint_end();

    paint_begin(1); box(0,0,320,240,BG);
    if (m->enabled) {
        /* The complete touch surface becomes the stop target, including its edges. */
        box(0,0,320,240,0xc4263b);
        paint_text_centered(160,120,44,WHITE,"DISABLE");
        paint_text_centered(160,164,13,WHITE,"TAP ANYWHERE TO STOP");
        paint_end();
        return;
    }
    button(8,6,72,30,"Drive",page==PAGE_DRIVE,false);
    button(85,6,72,30,"Setup",page==PAGE_SETUP,false);
    button(162,6,72,30,"Inputs",page==PAGE_INPUTS,false);
    button(239,6,73,30,"Wi-Fi",page==PAGE_WIFI,false);
    if (page == PAGE_DRIVE) {
        text(12,46,9,MUTED,"ROBOT MODE");
        button(12,61,296,31,modes[m->mode],false,false);
        text(280,69,13,TEAL,"v");
        text(12,103,9,MUTED,"ALLIANCE STATION");
        snprintf(s,sizeof s,"%s %u",m->alliance<3 ? "RED":"BLUE",m->alliance%3+1);
        button(12,118,296,31,s,false,false);
        text(280,126,13,TEAL,"v");
        bool ready=!m->wifi_busy && m->configured && m->connected && m->code && !m->estop && m->neutral;
        box(12,166,296,43,ready ? 0x17664f : PANEL);
        paint_text_centered(160,187.5f,25,ready ? WHITE : MUTED,"ENABLE");
        const char *reason=m->estop ? "Reset robot E-stop and restart app" :
            !m->configured ? "Set your team number in Setup" :
            !m->connected ? "Waiting for robot connection" :
            !m->code ? "Waiting for robot code" :
            !m->neutral ? "Center sticks and release robot buttons" : "Tap ENABLE when ready";
        text(12,219,11,ready ? TEAL : AMBER,reason);
        if(popup!=POPUP_NONE) {
            box(6,40,308,195,0x0c131d);
            text(16,45,11,MUTED,popup==POPUP_MODE ? "SELECT MODE":"SELECT ALLIANCE STATION");
            int count=popup==POPUP_MODE ? 3:6;
            int height=popup==POPUP_MODE ? 45:26;
            for(int i=0;i<count;i++) {
                if(popup==POPUP_MODE) snprintf(s,sizeof s,"%s",modes[i]);
                else snprintf(s,sizeof s,"%s %u",i<3 ? "RED":"BLUE",i%3+1);
                button(12,64+i*height,296,height-2,s,
                    popup==POPUP_MODE ? m->mode==i:m->alliance==i,false);
            }
        }
    } else if (page == PAGE_SETUP) {
        text(12,45,10,MUTED,"CONNECTION");
        button(12,64,296,45,"",false,m->enabled);
        text(22,69,9,MUTED,"TEAM NUMBER / TAP TO EDIT");
        if(m->configured) snprintf(s,sizeof s,"Team %u",m->team); else snprintf(s,sizeof s,"Not configured");
        text(22,84,16,WHITE,s);
        text(12,122,10,MUTED,"ROBOT ADDRESS (AUTOMATIC)");
        text(12,139,13,WHITE,m->configured ? m->target : "Enter your team number above");
        text(12,163,10,MUTED,"Configure your connection on the Wi-Fi tab.");
        text(12,178,10,MUTED,"2.4 GHz network / direct UDP connection");

    } else if(page==PAGE_WIFI) {
        snprintf(s,sizeof s,"Try: %.32s",m->wifi_suggestion && m->wifi_suggestion[0] ? m->wifi_suggestion:"set team in Setup");
        text(12,44,11,TEAL,s);
        if(m->wifi_suggestion && m->wifi_suggestion[0]) button(246,40,62,23,"Use",false,m->wifi_busy);
        button(12,68,296,34,"",false,m->wifi_busy);
        text(22,71,8,MUTED,"NETWORK NAME / TAP TO EDIT");
        text(22,83,13,WHITE,m->wifi_ssid && m->wifi_ssid[0] ? m->wifi_ssid:"Tap to enter SSID");
        button(12,108,296,32,"",false,m->wifi_busy);
        text(22,110,8,MUTED,"PASSWORD / TAP TO ENTER");
        text(22,122,12,WHITE,m->wifi_security==WIFI_OPEN ? "Not needed for open network":
            m->wifi_password_set ? "********":"Tap to enter password");
        button(12,147,190,26,m->wifi_security==WIFI_OPEN ? "Open network  v":"WPA2 Personal (AES)  v",false,m->wifi_busy);
        snprintf(s,sizeof s,"Slot %u  v",m->wifi_slot+1);
        button(210,147,98,26,s,false,m->wifi_busy);
        box(12,180,296,31,m->wifi_busy ? 0x572b36:0x17664f);
        paint_text_centered(160,195.5f,16,WHITE,m->wifi_busy ? "Cancel connection":"Save & connect");
        snprintf(s,sizeof s,"Replaces slot %u: %.29s",m->wifi_slot+1,m->wifi_slot_ssid && m->wifi_slot_ssid[0] ? m->wifi_slot_ssid:"empty");
        text(12,215,9,AMBER,s);
        snprintf(s,sizeof s,"%.58s",m->wifi_status ? m->wifi_status:"");
        text(12,229,8,m->wifi_available ? MUTED:RED,s);
        if(popup==POPUP_SECURITY || popup==POPUP_WIFI_SLOT) {
            box(0,38,320,202,BG);
            text(16,45,11,MUTED,popup==POPUP_SECURITY ? "SECURITY":"SAVE TO SYSTEM WI-FI SLOT");
            int count=popup==POPUP_SECURITY ? 2:3;
            for(int i=0;i<count;i++) {
                if(popup==POPUP_SECURITY) snprintf(s,sizeof s,"%s",i==0 ? "WPA2 Personal (AES)":"Open network");
                else snprintf(s,sizeof s,"Wi-Fi slot %u",i+1);
                button(12,64+i*45,296,43,s,popup==POPUP_SECURITY ? (int)m->wifi_security==i:(int)m->wifi_slot==i,false);
            }
        }
    } else {
        stick(16,47,"CIRCLE PAD",m->controls.axes[0],m->controls.axes[1]);
        stick(132,47,"C-STICK",m->controls.axes[2],m->controls.axes[3]);
        text(245,48,10,MUTED,"D-PAD");
        int pov=m->controls.pov;
        bool up=pov==0 || pov==45 || pov==315;
        bool right=pov==45 || pov==90 || pov==135;
        bool down=pov==135 || pov==180 || pov==225;
        bool left=pov==225 || pov==270 || pov==315;
        box(268,70,16,16,up ? TEAL:LINE);
        box(250,88,16,16,left ? TEAL:LINE);
        box(268,88,16,16,PANEL);
        box(286,88,16,16,right ? TEAL:LINE);
        box(268,106,16,16,down ? TEAL:LINE);
        const char *labels[]={"A","B","X","Y","L","R","ZR","ZL","SEL","STA"};
        for(int i=0;i<10;i++) button(12+60*(i%5),144+32*(i/5),54,28,labels[i],m->controls.buttons&(1u<<i),false);
        text(12,210,10,MUTED,"All physical controls go to joystick 0.");
    }
    paint_end();
}
static bool inside(int x,int y,int bx,int by,int w,int h) { return x>=bx && x<bx+w && y>=by && y<by+h; }
UiAction ui_touch(UiPage page,UiPopup popup,int x,int y,bool enabled) {
    if(x<0 || x>=320 || y<0 || y>=240) return UI_NONE;
    if(enabled) return UI_DISABLE;
    if(popup!=POPUP_NONE) {
        int count=popup==POPUP_MODE ? 3:popup==POPUP_ALLIANCE ? 6:popup==POPUP_SECURITY ? 2:3;
        int height=popup==POPUP_ALLIANCE ? 26:45;
        UiAction first=popup==POPUP_MODE ? UI_TELEOP:popup==POPUP_ALLIANCE ? UI_RED1:popup==POPUP_SECURITY ? UI_SECURITY_WPA2:UI_SLOT1;
        for(int i=0;i<count;i++) if(inside(x,y,12,64+i*height,296,height-2)) return first+i;
        return UI_CLOSE_POPUP;
    }
    for(int i=0;i<4;i++) if(inside(x,y,8+77*i,6,i==3 ? 73:72,30)) return UI_TAB_DRIVE+i;
    if(page==PAGE_DRIVE) {
        if(inside(x,y,12,61,296,31)) return UI_OPEN_MODE;
        if(inside(x,y,12,118,296,31)) return UI_OPEN_ALLIANCE;
        if(inside(x,y,12,166,296,43)) return UI_ENABLE;
    }
    if(page==PAGE_SETUP) {
        if(inside(x,y,12,64,296,45)) return UI_TEAM;

    }
    if(page==PAGE_WIFI) {
        if(inside(x,y,246,40,62,23)) return UI_WIFI_SUGGEST;
        if(inside(x,y,12,68,296,34)) return UI_WIFI_SSID;
        if(inside(x,y,12,108,296,32)) return UI_WIFI_PASSWORD;
        if(inside(x,y,12,147,190,26)) return UI_OPEN_SECURITY;
        if(inside(x,y,210,147,98,26)) return UI_OPEN_WIFI_SLOT;
        if(inside(x,y,12,180,296,31)) return UI_WIFI_CONNECT;
    }
    return UI_NONE;
}
