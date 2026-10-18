#include "ui.h"
#include <stdio.h>
#include <string.h>
#define BG 0x0c131d
#define PANEL 0x162332
#define LINE 0x25384b
#define WHITE 0xecf4fa
#define MUTED 0x94aabd
#define TEAL 0x49dfbd
#define AMBER 0xffcf75
#define RED 0xff6b78
static void text(float x, float y, float size, uint32_t color, const char *s) {
    paint_text(x, y, size, color, s);
}
static void box(float x, float y, float w, float h, uint32_t color) { paint_box(x,y,w,h,color); }
static void ssid_label(char out[33],const char *ssid) {
    unsigned i=0;
    for(;i<32 && ssid && ssid[i];i++) {
        unsigned char c=(unsigned char)ssid[i];
        out[i]=c>=32 && c<=126 ? (char)c:'?';
    }
    out[i]=0;
}
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
    snprintf(s,sizeof s,"TELEOP  /  %s %u",m->alliance < 3 ? "RED" : "BLUE",m->alliance%3+1);
    text(14,176,12,WHITE,s);
    text(14,195,11,MUTED,m->configured ? m->target : "Set your team number on Drive");
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
    button(8,6,70,30,"Drive",page==PAGE_DRIVE,false);
    button(86,6,70,30,"Inputs",page==PAGE_INPUTS,false);
    button(164,6,70,30,"Wi-Fi",page==PAGE_WIFI,false);
    button(242,6,70,30,"Feed",page==PAGE_FEED,false);
    if (page == PAGE_FEED) {
        box(8,42,304,192,LINE);
        box(9,43,302,190,PANEL);
        for(unsigned i=0;i<FEED_LINES;i++)
            paint_feed_line(14,47+i*10.5f,292,10,WHITE,m->feed ? m->feed->lines[i] : i==0 ? "Waiting for roboRIO console...":"");
    } else if (page == PAGE_DRIVE) {
        text(12,46,9,MUTED,"ALLIANCE STATION");
        snprintf(s,sizeof s,"%s %u",m->alliance<3 ? "RED":"BLUE",m->alliance%3+1);
        button(12,61,296,31,s,false,false);
        text(280,69,13,TEAL,"v");
        text(12,103,11,MUTED,"Team Number:");
        button(12,118,296,31,"",false,false);
        if(m->configured) {
            snprintf(s,sizeof s,"%u",m->team);
            text(20,126,16,WHITE,s);
        }
        bool ready=m->wifi && !m->wifi_busy && m->configured && m->connected && m->code && !m->estop && m->neutral;
        box(12,166,296,43,ready ? 0x17664f : PANEL);
        paint_text_centered(160,187.5f,25,ready ? WHITE : MUTED,"ENABLE");
        const char *reason=m->estop ? "Reset robot E-stop and restart app" :
            !m->configured ? "Set your team number on Drive" :
            m->wifi_busy ? "Wait for Wi-Fi operation to finish" :
            !m->wifi ? "Connect to Wi-Fi first" :
            !m->connected ? "Waiting for robot connection" :
            !m->code ? "Waiting for robot code" :
            !m->neutral ? "Center sticks and release robot buttons" : "Tap ENABLE when ready";
        text(12,219,11,ready ? TEAL : AMBER,reason);
        if(popup==POPUP_ALLIANCE) {
            box(6,40,308,195,BG);
            text(16,45,11,MUTED,"SELECT ALLIANCE STATION");
            for(int i=0;i<6;i++) {
                snprintf(s,sizeof s,"%s %u",i<3 ? "RED":"BLUE",i%3+1);
                button(12,64+i*26,296,24,s,m->alliance==i,false);
            }
        }
    } else if(page==PAGE_WIFI) {
        /* SSIDs are arbitrary bytes, not guaranteed UTF-8 for Citro2D. Keep
         * their original bytes for joining and sanitize only display labels. */
        char selected_ssid[33],suggestion[33];
        ssid_label(selected_ssid,m->wifi_ssid); ssid_label(suggestion,m->wifi_suggestion);
        bool found=m->wifi_suggestion && m->wifi_suggestion[0];
        text(12,44,9,found ? TEAL:MUTED,found ? "Join Recommended Network:":"No team network found in scan");
        if(found) {
            text(12,60,13,WHITE,suggestion);
            box(240,43,68,37,0x17664f);
            paint_text_centered(274,61.5f,16,WHITE,"Join");
        }
        snprintf(s,sizeof s,"%.23s%s",selected_ssid[0] ? selected_ssid:"Select network  v",strlen(selected_ssid)>23 ? "...":"");
        button(12,89,240,32,s,false,m->wifi_busy);
        button(260,89,48,32,m->wifi_scanning ? "...":"Scan",false,m->wifi_busy);
        button(12,129,240,32,"",false,m->wifi_busy);
        text(20,131,8,MUTED,"PASSWORD / TAP TO EDIT");
        const char *password=m->wifi_security==WIFI_OPEN && m->wifi_ssid && m->wifi_ssid[0] ? "Not required":
            m->wifi_show_password && m->wifi_password && m->wifi_password[0] ? m->wifi_password:
            m->wifi_password_set ? "********":"Tap to enter password";
        snprintf(s,sizeof s,"%.24s%s",password,strlen(password)>24 ? "...":"");
        text(20,144,11,WHITE,s);
        button(260,129,48,32,m->wifi_show_password ? "Hide":"Show",false,m->wifi_busy);
        snprintf(s,sizeof s,"Detected security: %s",m->wifi_ssid && m->wifi_ssid[0] ? wifi_security_name(m->wifi_security):"select a network");
        text(12,171,10,MUTED,s);
        box(12,194,296,31,m->wifi_busy ? 0x572b36:0x17664f);
        paint_text_centered(160,209.5f,16,WHITE,m->wifi_scanning ? "Cancel scan":m->wifi_busy ? "Cancel connection":"Join selected network");
        snprintf(s,sizeof s,"%.58s",m->wifi_status ? m->wifi_status:"");
        text(12,230,8,m->wifi_available ? MUTED:RED,s);
        if(popup==POPUP_NETWORKS) {
            box(0,38,320,202,BG);
            text(16,45,11,MUTED,"FOUND NETWORKS / AUTOMATIC SECURITY");
            for(unsigned i=0;i<5;i++) {
                unsigned index=m->network_page*5+i;
                if(index>=m->network_count) break;
                const WifiNetwork *n=&m->networks[index];
                char label[33]; ssid_label(label,n->ssid);
                box(12,64+i*27,296,25,PANEL);
                box(12,87+i*27,296,2,LINE);
                snprintf(s,sizeof s,"%.24s%s",label,strlen(label)>24 ? "...":"");
                text(22,66+i*27,11,wifi_security_supported(n->security) ? WHITE:MUTED,s);
                text(22,80+i*27,7,MUTED,wifi_security_name(n->security));
            }
            if(!m->network_count) text(16,93,12,MUTED,m->wifi_scanning ? "Scanning...":"No networks found. Tap Scan to retry.");
            button(12,207,80,27,"Previous",false,false);
            button(228,207,80,27,"Next",false,false);
            snprintf(s,sizeof s,"Page %u",m->network_page+1);
            text(135,214,10,MUTED,s);
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
        if(popup==POPUP_NETWORKS) {
            for(int i=0;i<5;i++) if(inside(x,y,12,64+i*27,296,25)) return UI_NETWORK0+i;
            if(inside(x,y,12,207,80,27)) return UI_NETWORK_PREV;
            if(inside(x,y,228,207,80,27)) return UI_NETWORK_NEXT;
        } else {
            for(int i=0;i<6;i++) if(inside(x,y,12,64+i*26,296,24)) return UI_RED1+i;
        }
        return UI_CLOSE_POPUP;
    }
    for(int i=0;i<4;i++) if(inside(x,y,8+78*i,6,70,30)) return UI_TAB_DRIVE+i;
    if(page==PAGE_DRIVE) {
        if(inside(x,y,12,61,296,31)) return UI_OPEN_ALLIANCE;
        if(inside(x,y,12,118,296,31)) return UI_TEAM;
        if(inside(x,y,12,166,296,43)) return UI_ENABLE;
    }
    if(page==PAGE_WIFI) {
        if(inside(x,y,240,43,68,37)) return UI_WIFI_SUGGEST;
        if(inside(x,y,12,89,240,32)) return UI_WIFI_NETWORKS;
        if(inside(x,y,260,89,48,32)) return UI_WIFI_SCAN;
        if(inside(x,y,12,129,240,32)) return UI_WIFI_PASSWORD;
        if(inside(x,y,260,129,48,32)) return UI_WIFI_SHOW;
        if(inside(x,y,12,194,296,31)) return UI_WIFI_CONNECT;
    }
    return UI_NONE;
}
