#include <citro2d.h>
#include "ui.h"
static C3D_RenderTarget *screens[2];
static C2D_TextBuf text_buffer;
static bool c3d_ready, c2d_ready, frame;
static u32 color(uint32_t rgb) { return C2D_Color32(rgb>>16,(rgb>>8)&255,rgb&255,255); }
bool display_init(void) {
    c3d_ready=C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    if(!c3d_ready) return false;
    c2d_ready=C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    if(!c2d_ready) { display_exit(); return false; }
    C2D_Prepare();
    screens[0]=C2D_CreateScreenTarget(GFX_TOP,GFX_LEFT);
    screens[1]=C2D_CreateScreenTarget(GFX_BOTTOM,GFX_LEFT);
    text_buffer=C2D_TextBufNew(2048);
    if(!screens[0] || !screens[1] || !text_buffer) { display_exit(); return false; }
    return true;
}
void display_exit(void) {
    if(text_buffer) { C2D_TextBufDelete(text_buffer); text_buffer=NULL; }
    if(c2d_ready) { C2D_Fini(); c2d_ready=false; }
    if(c3d_ready) { C3D_Fini(); c3d_ready=false; }
}
void paint_begin(int screen) {
    if(screen==0) {
        /* Don't block the network loop waiting for a vertical blank or busy GPU. */
        frame=C3D_FrameBegin(C3D_FRAME_NONBLOCK);
        if(frame) C2D_TextBufClear(text_buffer);
    }
    if(!frame) return;
    C2D_TargetClear(screens[screen],color(0x0c131d));
    C2D_SceneBegin(screens[screen]);
}
void paint_box(float x,float y,float w,float h,uint32_t rgb) {
    if(frame && w>0 && h>0) C2D_DrawRectSolid(x,y,0,w,h,color(rgb));
}
void paint_text(float x,float y,float size,uint32_t rgb,const char *value) {
    if(!frame) return;
    C2D_Text text;
    C2D_TextParse(&text,text_buffer,value);
    C2D_DrawText(&text,C2D_WithColor,x,y,0,size/30.0f,size/30.0f,color(rgb));
}
void paint_feed_line(float x,float y,float width,float size,uint32_t rgb,const char *value) {
    if(!frame) return;
    C2D_Text text;
    C2D_TextParse(&text,text_buffer,value);
    float w;
    C2D_TextGetDimensions(&text,size/30.0f,size/30.0f,&w,NULL);
    float sx=w>width ? size/30.0f*width/w : size/30.0f;
    C2D_DrawText(&text,C2D_WithColor,x,y,0,sx,size/30.0f,color(rgb));
}
void paint_text_centered(float cx,float cy,float size,uint32_t rgb,const char *value) {
    if(!frame) return;
    C2D_Text text;
    C2D_TextParse(&text,text_buffer,value);
    float w;
    C2D_TextGetDimensions(&text,size/30.0f,size/30.0f,&w,NULL);
    /* Citro2D normalizes glyph cells to 30px; line-feed height includes extra leading. */
    C2D_DrawText(&text,C2D_WithColor,cx-w/2,cy-size/2,0,size/30.0f,size/30.0f,color(rgb));
}
void paint_end(void) {
    /* ui_draw calls this after each screen; finish only after bottom. */
    static bool top_done;
    if(!top_done) top_done=true;
    else { if(frame) C3D_FrameEnd(0); top_done=false; }
}
