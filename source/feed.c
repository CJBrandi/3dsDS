#include "feed.h"
#include <stdio.h>
#include <string.h>
static unsigned u16(const uint8_t *p) { return (unsigned)p[0]*256+p[1]; }
void feed_clear(Feed *f) { memset(f,0,sizeof *f); }
void feed_reset_stream(Feed *f) { f->used=f->expected=f->header_used=0; }
static void newline(Feed *f) {
    if(f->row+1<FEED_LINES) f->row++;
    else memmove(f->lines,f->lines+1,sizeof f->lines-sizeof f->lines[0]);
    memset(f->lines[f->row],0,sizeof f->lines[0]); f->column=0;
}
static void append(Feed *f,const uint8_t *p,size_t n) {
    for(size_t i=0;i<n;i++) {
        unsigned char c=p[i];
        if(c=='\r') continue;
        if(c=='\n') { newline(f); continue; }
        if(c=='\t') c=' ';
        /* Console control bytes cannot become renderer commands or invalid UTF-8. */
        if(c<32 || c>126) c='?';
        if(f->column==FEED_COLUMNS) newline(f);
        f->lines[f->row][f->column++]=(char)c;
    }
}
void feed_text(Feed *f,const char *s) { append(f,(const uint8_t *)s,strlen(s)); }
static void frame(Feed *f) {
    const uint8_t *p=f->frame;
    size_t n=f->expected;
    if(!n) return; /* TCP keepalive. */
    if(p[0]==12 && n>=7) append(f,p+7,n-7);
    else if(p[0]==11 && n>=20) {
        /* Validate all three length-prefixed fields before displaying any of them. */
        size_t start[3],length[3],at=14;
        for(unsigned i=0;i<3;i++) {
            if(n-at<2) return;
            length[i]=u16(p+at); at+=2; start[i]=at;
            if(length[i]>n-at) return;
            at+=length[i];
        }
        if(f->column) newline(f);
        char label[48];
        uint32_t code=(uint32_t)p[9]<<24 | (uint32_t)p[10]<<16 | (uint32_t)p[11]<<8 | p[12];
        snprintf(label,sizeof label,"%s %ld: ",p[13]&1 ? "ERROR":"WARNING",(long)(int32_t)code);
        feed_text(f,label);
        for(unsigned i=0;i<3;i++) if(length[i]) {
            append(f,p+start[i],length[i]);
            if(f->column) newline(f);
        }
    }
}
void feed_receive(Feed *f,const uint8_t *p,size_t n) {
    while(n) {
        if(f->header_used<2) {
            f->header[f->header_used++]=*p++; n--;
            if(f->header_used<2) continue;
            f->expected=u16(f->header);
            if(!f->expected) { feed_reset_stream(f); continue; }
        }
        size_t count=f->expected-f->used;
        if(count>n) count=n;
        memcpy(f->frame+f->used,p,count); f->used+=count; p+=count; n-=count;
        if(f->used==f->expected) { frame(f); feed_reset_stream(f); }
    }
}
