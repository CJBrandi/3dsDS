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
void ui_draw(const UiModel *m, UiPage page) {
    char s[96];
    paint_begin(0); box(0,0,400,240,BG);
    text(14,10,19,WHITE,"3dsDS"); text(93,15,10,MUTED,"ROBOT CONTROL");
    box(289,10,97,23,m->connected ? 0x174d48 : PANEL);
    text(299,16,10,m->connected ? TEAL : MUTED,m->connected ? "CONNECTED" : "OFFLINE");
    box(14,45,372,65,PANEL);
    uint32_t state = m->estop ? RED : m->enabled ? TEAL : AMBER;
    box(14,45,4,65,state);
    text(28,53,10,MUTED,m->arming ? "HOLD START TO ARM" : "DRIVER STATION");
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
    text(14,195,11,MUTED,m->configured ? m->target : "Set your team or robot IP in Setup");
    text(246,176,10,MUTED,"ROBOT REPORTS");
    text(246,193,12,m->connected && m->rio_enabled ? TEAL : MUTED,
         !m->connected ? "Unknown" : m->rio_enabled ? "Enabled" : "Disabled");
    box(0,221,400,19,0x111e2b);
    snprintf(s,sizeof s,"%.65s",m->message);
    text(12,225,9,MUTED,s);
    paint_end();

    paint_begin(1); box(0,0,320,240,BG);
    button(8,6,98,30,"Drive",page==PAGE_DRIVE,false);
    button(111,6,98,30,"Setup",page==PAGE_SETUP,false);
    button(214,6,98,30,"Inputs",page==PAGE_INPUTS,false);
    if (page == PAGE_DRIVE) {
        text(12,43,9,MUTED,m->enabled ? "MODE LOCKED WHILE ENABLED" : "ROBOT MODE");
        for (int i=0;i<3;i++) button(12+102*i,57,94,30,modes[i],m->mode==i,m->enabled);
        text(12,95,9,MUTED,"ALLIANCE STATION");
        for (int i=0;i<6;i++) {
            snprintf(s,sizeof s,"%c%u",i<3 ? 'R':'B',i%3+1);
            button(12+51*i,109,43,27,s,m->alliance==i,m->enabled);
        }
        box(12,146,194,39,0x244055); text(23,157,18,WHITE,"DISABLE");
        box(214,146,94,39,0x572b36); text(224,159,13,RED,"E-STOP");
        if (m->estop) {
            text(12,194,11,RED,"E-stop latched. Reset robot + restart app.");
        } else {
            text(12,194,12,m->deadman ? TEAL : WHITE,m->enabled ? "Keep ZL held to drive" : "Hold ZL + START for 1 second");
            text(12,212,10,MUTED,"Release ZL or press SELECT to disable");
        }
        box(12,232,296,3,LINE);
        float progress=m->arm_progress; if(progress<0)progress=0; if(progress>1)progress=1;
        box(12,232,296*progress,3,TEAL);
    } else if (page == PAGE_SETUP) {
        text(12,45,10,MUTED,m->enabled ? "DISABLE ROBOT TO EDIT SETTINGS" : "CONNECTION");
        button(12,64,296,45,"",false,m->enabled);
        text(22,69,9,MUTED,"ROBOT IPv4 / TAP TO EDIT");
        text(22,84,16,WHITE,m->configured ? m->target : "Not configured");
        button(12,119,296,33,"Set address from team number",false,m->enabled);
        text(12,163,10,MUTED,"Join robot Wi-Fi in System Settings.");
        text(12,178,10,MUTED,"2.4 GHz network / direct UDP connection");
        button(12,199,296,30,"Quit to launcher",false,m->enabled);
    } else {
        stick(16,47,"CIRCLE PAD",m->controls.axes[0],m->controls.axes[1]);
        stick(132,47,"C-STICK",m->controls.axes[2],m->controls.axes[3]);
        text(245,48,10,MUTED,"POV");
        snprintf(s,sizeof s,"%d",m->controls.pov); text(245,69,19,WHITE,s);
        text(245,104,9,MUTED,"JOYSTICK 0");
        const char *labels[]={"A","B","X","Y","L","R","ZR"};
        for(int i=0;i<7;i++) button(12+43*i,147,38,30,labels[i],m->controls.buttons&(1u<<i),false);
        text(12,188,11,m->deadman ? TEAL : AMBER,m->deadman ? "ZL hold-to-run: HELD" : "ZL hold-to-run: RELEASED");
        text(12,207,10,MUTED,"START + ZL: arm    SELECT: disable");
        text(12,222,9,MUTED,"L + R + SELECT: emergency stop");
    }
    paint_end();
}
static bool inside(int x,int y,int bx,int by,int w,int h) { return x>=bx && x<bx+w && y>=by && y<by+h; }
UiAction ui_touch(UiPage page,int x,int y,bool enabled) {
    for(int i=0;i<3;i++) if(inside(x,y,8+103*i,6,98,30)) return UI_TAB_DRIVE+i;
    if(page==PAGE_DRIVE) {
        if(inside(x,y,12,146,194,39)) return UI_DISABLE;
        if(inside(x,y,214,146,94,39)) return UI_ESTOP;
        if(!enabled) {
            for(int i=0;i<3;i++) if(inside(x,y,12+102*i,57,94,30)) return UI_TELEOP+i;
            for(int i=0;i<6;i++) if(inside(x,y,12+51*i,109,43,27)) return UI_RED1+i;
        }
    }
    if(page==PAGE_SETUP && !enabled) {
        if(inside(x,y,12,64,296,45)) return UI_IP;
        if(inside(x,y,12,119,296,33)) return UI_TEAM;
        if(inside(x,y,12,199,296,30)) return UI_QUIT;
    }
    return UI_NONE;
}
