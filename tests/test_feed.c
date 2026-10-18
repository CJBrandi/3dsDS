#include "feed.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static Feed f;
static void packet(const uint8_t *p,size_t n) {
    uint8_t header[]={(uint8_t)(n>>8),(uint8_t)n};
    feed_receive(&f,header,1); feed_receive(&f,header+1,1);
    for(size_t i=0;i<n;i++) feed_receive(&f,p+i,1);
}
int main(void) {
    const uint8_t print[]={12,0,0,0,0,0,1,'h','i','\n'};
    packet(print,sizeof print);
    assert(!strcmp(f.lines[0],"hi"));
    /* Coalesced keepalives and multiple prints. */
    uint8_t combined[]={0,0,0,8,12,0,0,0,0,0,2,'A',0,8,12,0,0,0,0,0,3,'B'};
    feed_receive(&f,combined,sizeof combined);
    assert(!strcmp(f.lines[1],"AB"));
    uint8_t error[]={11,0,0,0,0,0,4,0,1,0xff,0xff,0xff,0xff,1,0,3,'b','a','d',0,3,'l','o','c',0,5,'s','t','a','c','k'};
    packet(error,sizeof error);
    assert(!strcmp(f.lines[2],"ERROR -1: bad"));
    assert(!strcmp(f.lines[3],"loc")); assert(!strcmp(f.lines[4],"stack"));
    error[13]=0; packet(error,sizeof error);
    assert(!strcmp(f.lines[5],"WARNING -1: bad"));
    unsigned row=f.row;
    error[14]=255; packet(error,sizeof error); assert(f.row==row);
    /* Abandoned partial frames cannot leak across connections. */
    uint8_t partial[]={0,100,12,1}; feed_receive(&f,partial,sizeof partial);
    feed_reset_stream(&f); packet(print,sizeof print); assert(!strcmp(f.lines[row],"hi"));
    feed_clear(&f);
    for(unsigned i=0;i<FEED_COLUMNS+1;i++) feed_text(&f,"x");
    assert(strlen(f.lines[0])==FEED_COLUMNS && !strcmp(f.lines[1],"x"));
    for(unsigned i=0;i<100;i++) feed_text(&f,"\nline");
    assert(f.row==FEED_LINES-1 && !strcmp(f.lines[f.row],"line"));
    /* Maximum-size unknown frame and malformed short known frames. */
    static uint8_t large[65535]; large[0]=99; packet(large,sizeof large);
    uint8_t short_frame[]={11,0}; packet(short_frame,sizeof short_frame);
    feed_clear(&f); feed_text(&f,"a\t\033\xff\rb"); assert(!strcmp(f.lines[0],"a ??b"));
    puts("PASS: fragmented/coalesced console frames, errors/warnings, malformed lengths, reconnect reset and bounded text");
}
