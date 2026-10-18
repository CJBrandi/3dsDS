/* Render the actual UI layout as SVG on a host for layout inspection. */
#include <stdio.h>
#include <stdlib.h>
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
    fprintf(file,"<text x=\"%g\" y=\"%g\" font-family=\"sans-serif\" font-size=\"%g\" fill=\"#%06x\">",x,y+size*.82f,size,c);
    escaped(text);fputs("</text>",file);
}
void paint_end(void) { fputs("</svg>",file); fclose(file); }
int main(int argc,char **argv) {
    directory=argc>1 ? argv[1]:"docs";
    UiModel m={.connected=true,.wifi=true,.code=true,.configured=true,.voltage=12.54f,
        .age_ms=18,.target="10.12.34.2",.message="Ready. Hold ZL + START to arm.",
        .controls={{25,-40,0,0},5,-1}};
    UiPage page=argc>2 ? (UiPage)atoi(argv[2]):PAGE_DRIVE;
    if(argc>3) { m.enabled=true; m.rio_enabled=true; }
    ui_draw(&m,page);
    return 0;
}
