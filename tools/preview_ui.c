/* Render the actual UI layout as SVG on a host for layout inspection. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui.h"
static FILE *file;
static const char *directory;
static void escaped(const char *s) {
    for(;*s;s++) {
        if(*s=='&')fputs("&amp;",file);
        else if(*s=='<')fputs("&lt;",file);
        else if(*s=='>')fputs("&gt;",file);
        else fputc(*s,file);
    }
}
void paint_begin(int screen) {
    char name[256]; snprintf(name,sizeof name,"%s/%s.svg",directory,screen==0 ? "top":"bottom");
    file=fopen(name,"w"); if(!file)exit(1);
    fprintf(file,"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"240\" viewBox=\"0 0 %d 240\">",screen ? 320:400,screen ? 320:400);
}
void paint_box(float x,float y,float w,float h,uint32_t c) {
    fprintf(file,"<rect x=\"%g\" y=\"%g\" width=\"%g\" height=\"%g\" fill=\"#%06x\"/>",x,y,w,h,c);
}
void paint_text(float x,float y,float size,uint32_t c,const char *text) {
    fprintf(file,"<text x=\"%g\" y=\"%g\" font-family=\"DejaVu Sans\" font-size=\"%g\" fill=\"#%06x\">",x,y+size*.82f,size,c);
    escaped(text);fputs("</text>",file);
}
void paint_text_centered(float cx,float cy,float size,uint32_t c,const char *text) {
    fprintf(file,"<text x=\"%g\" y=\"%g\" text-anchor=\"middle\" data-center-y=\"%g\" font-family=\"DejaVu Sans\" font-size=\"%g\" fill=\"#%06x\">",cx,cy,cy,size,c);
    escaped(text);fputs("</text>",file);
}
void paint_end(void) { fputs("</svg>",file); fclose(file); }
int main(int argc,char **argv) {
    directory=argc>1 ? argv[1]:"docs";
    WifiNetwork networks[]={ {.ssid="FRC-1234",.security=WIFI_WPA2}, {.ssid="FRC-AP-1234-A",.security=WIFI_WPA2}, {.ssid="Workshop",.security=WIFI_OPEN}, {.ssid="Radio WPA3",.security=WIFI_WPA3}, {.ssid="Guest",.security=WIFI_OPEN} };
    UiModel m={.connected=true,.wifi=true,.code=true,.configured=true,.voltage=12.54f,
        .age_ms=18,.neutral=true,.team=1234,.target="10.12.34.2",.message="Ready. Tap ENABLE on the Drive screen.",
        .wifi_ssid="FRC-1234",.wifi_suggestion="FRC-1234",.wifi_available=true,
        .wifi_status="Remembers only your last used network.",.wifi_password_set=true,.wifi_password="robotpass",.networks=networks,.network_count=5,
        .controls={{0,0,0,0},0,-1}};
    UiPage page=argc>2 ? (UiPage)atoi(argv[2]):PAGE_DRIVE;
    UiPopup popup=POPUP_NONE;
    if(argc>3) {
        if(!strcmp(argv[3],"enabled")) { m.enabled=true; m.rio_enabled=true; }
        if(!strcmp(argv[3],"mode")) popup=POPUP_MODE;
        if(!strcmp(argv[3],"alliance")) popup=POPUP_ALLIANCE;
        if(!strcmp(argv[3],"networks")) popup=POPUP_NETWORKS;
        if(!strcmp(argv[3],"show")) m.wifi_show_password=true;
        if(!strcmp(argv[3],"missing")) { m.wifi_suggestion=NULL; m.wifi_ssid=""; m.wifi_password_set=false; }
    }
    if(page==PAGE_INPUTS) { m.controls=(Controls){{25,-40,0,0},5,45}; m.neutral=false; }
    ui_draw(&m,page,popup);
    return 0;
}
