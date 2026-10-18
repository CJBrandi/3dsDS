#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "wifi_scan.h"
static unsigned char data[512];
static void put32(size_t at,unsigned v) { for(unsigned i=0;i<4;i++) data[at+i]=(unsigned char)(v>>(i*8)); }
static size_t frame(unsigned akm,bool transition) {
    memset(data,0,sizeof data);put32(0,sizeof data);put32(8,1);
    data[17]=6;data[20]=2;
    put32(36,28);
    size_t at=40;
    data[at]=0x80;data[at+16]=2;at+=24;
    data[at+10]=akm ? 0x10:0;at+=12;
    data[at++]=0;data[at++]=8;memcpy(data+at,"FRC-1234",8);at+=8;
    if(akm) {
        unsigned char rsn[]={1,0,0,15,172,4,1,0,0,15,172,4,1,0,0,15,172,2,0,0};
        rsn[17]=(unsigned char)akm;
        data[at++]=48;data[at++]=transition ? 24:20;
        if(transition) { rsn[12]=2;memcpy(data+at,rsn,18);at+=18;
            memcpy(data+at,"\x00\x0f\xac\x08\x00\x00",6);at+=6;
        } else {memcpy(data+at,rsn,20);at+=20;}
    }
    put32(12,at-12);put32(32,at-12);put32(4,at);put32(0,at);return at;
}
int main(void) {
    WifiNetwork n[4];size_t count;
    size_t size=frame(0,false);
    assert(wifi_parse_beacons(data,size,n,4,&count) && count==1 && n[0].security==WIFI_OPEN);
    size=frame(2,false);assert(wifi_parse_beacons(data,size,n,4,&count) && count==1 && n[0].security==WIFI_WPA2);
    size=frame(8,false);assert(wifi_parse_beacons(data,size,n,4,&count) && n[0].security==WIFI_WPA3);
    size=frame(1,false);assert(wifi_parse_beacons(data,size,n,4,&count) && n[0].security==WIFI_ENTERPRISE);
    size=frame(2,true);assert(wifi_parse_beacons(data,size,n,4,&count) && n[0].security==WIFI_WPA2);
    data[size-2]=0x40;assert(wifi_parse_beacons(data,size,n,4,&count) && n[0].security==WIFI_UNSUPPORTED);
    size=frame(2,false);data[size-8]=255;assert(wifi_parse_beacons(data,size,n,4,&count) && n[0].security==WIFI_UNSUPPORTED);
    size=frame(0,false);data[77]=33;assert(wifi_parse_beacons(data,size,n,4,&count) && count==0);
    size=frame(2,false);put32(12,0xffffffff);assert(!wifi_parse_beacons(data,size,n,4,&count));
    for(size_t i=0;i<size;i++) {frame(2,false);assert(!wifi_parse_beacons(data,i,n,4,&count));}
    assert(wifi_team_score("FRC-1234",1234)>wifi_team_score("FRC-AP-1234-A",1234));
    assert(wifi_team_score("FRC-12345",1234)==0);
    assert(wifi_team_score("FRC-1234-A",1234)>0);
    assert(wifi_team_score("FRC-1234",0)==0);
    assert(!wifi_security_supported(WIFI_WPA3));
    puts("PASS: bounded beacon parsing, automatic security, unsupported modes, team-name boundaries");
}
