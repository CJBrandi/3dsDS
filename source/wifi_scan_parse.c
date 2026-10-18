#include "wifi_scan.h"
#include <stdio.h>
#include <string.h>
static uint16_t le16(const uint8_t *p) { return p[0]|((uint16_t)p[1]<<8); }
static uint32_t le32(const uint8_t *p) { return le16(p)|((uint32_t)le16(p+2)<<16); }
bool wifi_security_supported(WifiSecurity s) { return s==WIFI_OPEN || s==WIFI_WPA2; }
const char *wifi_security_name(WifiSecurity s) {
    switch(s) {
        case WIFI_OPEN:return "Open";
        case WIFI_WPA2:return "WPA2 Personal (AES)";
        case WIFI_WPA3:return "WPA3 only (unsupported)";
        case WIFI_ENTERPRISE:return "Enterprise (unsupported)";
        default:return "Unsupported security";
    }
}
/* RSN suites use IEEE OUI 00:0f:ac. Never infer Open from a bad RSN. */
static WifiSecurity rsn_security(const uint8_t *p,size_t n) {
    if(n<8 || le16(p)!=1) return WIFI_UNSUPPORTED;
    const uint8_t oui[]={0,15,172};
    if(memcmp(p+2,oui,3) || p[5]!=4) return WIFI_UNSUPPORTED;
    size_t at=6;
    unsigned pairs=le16(p+at); at+=2;
    if(pairs>(n-at)/4) return WIFI_UNSUPPORTED;
    bool aes=false;
    for(unsigned i=0;i<pairs;i++,at+=4) if(!memcmp(p+at,oui,3) && p[at+3]==4) aes=true;
    if(n-at<2) return WIFI_UNSUPPORTED;
    unsigned akms=le16(p+at);at+=2;
    if(akms>(n-at)/4) return WIFI_UNSUPPORTED;
    bool psk=false,sae=false,enterprise=false;
    for(unsigned i=0;i<akms;i++,at+=4) if(!memcmp(p+at,oui,3)) {
        if(p[at+3]==2) psk=true;
        if(p[at+3]==8) sae=true;
        if(p[at+3]==1 || p[at+3]==5) enterprise=true;
    }
    if(n-at==1) return WIFI_UNSUPPORTED;
    bool pmf_required=n-at>=2 && (le16(p+at)&0x40);
    if(n-at>=2) at+=2; /* RSN capabilities. */
    if(n-at) {
        if(n-at<2) return WIFI_UNSUPPORTED;
        unsigned pmkids=le16(p+at); at+=2;
        if(pmkids>(n-at)/16) return WIFI_UNSUPPORTED;
        at+=pmkids*16;
    }
    /* Only the optional four-byte group management cipher may remain. */
    if(n-at && n-at!=4) return WIFI_UNSUPPORTED;
    if(aes && psk && !pmf_required) return WIFI_WPA2;
    if(sae && !psk) return WIFI_WPA3;
    if(enterprise && !psk) return WIFI_ENTERPRISE;
    return WIFI_UNSUPPORTED;
}
static bool beacon(const uint8_t *p,size_t n,const uint8_t *mac,WifiNetwork *out) {
    if(n<36 || (p[0]!=0x80 && p[0]!=0x50) || memcmp(p+16,mac,6)) return false;
    size_t start=24;
    unsigned capabilities=le16(p+start+10);
    if(!(capabilities&1) || (capabilities&2)) return false; /* Infrastructure only. */
    bool privacy=(capabilities&0x10)!=0;
    size_t at=start+12;
    bool ssid=false,rsn=false,wpa=false;
    out->security=privacy ? WIFI_UNSUPPORTED:WIFI_OPEN;
    while(at<n) {
        if(n-at<2) return false;
        uint8_t id=p[at],len=p[at+1];at+=2;
        if(len>n-at) return false;
        if(id==0) {
            if(ssid || !len || len>32) return false;
            for(unsigned i=0;i<len;i++) if(p[at+i]<32 || p[at+i]==127) return false;
            memcpy(out->ssid,p+at,len);out->ssid[len]=0;ssid=true;
        } else if(id==48) {
            if(rsn) return false;
            rsn=true;out->security=rsn_security(p+at,len);
        } else if(id==221 && len>=4 && !memcmp(p+at,"\x00\x50\xf2\x01",4)) wpa=true;
        at+=len;
    }
    if(wpa && !rsn) out->security=WIFI_UNSUPPORTED;
    if(rsn && !privacy) out->security=WIFI_UNSUPPORTED;
    return ssid;
}
bool wifi_parse_beacons(const uint8_t *p,size_t n,WifiNetwork *out,size_t cap,size_t *count) {
    *count=0;
    if(!p || n<12 || !out) return false;
    size_t max=le32(p),used=le32(p+4),entries=le32(p+8),at=12;
    if(max>n || used>max || used<12 || entries>(used-12)/28) return false;
    for(size_t i=0;i<entries;i++) {
        if(used-at<28) return false;
        size_t size=le32(p+at),offset=le32(p+at+24);
        if(size<28 || size>used-at || offset<28 || offset>size) return false;
        WifiNetwork net={0};memcpy(net.bssid,p+at+8,6);net.channel=p[at+5];
        if(net.channel>=1 && net.channel<=14 && beacon(p+at+offset,size-offset,net.bssid,&net)) {
            bool duplicate=false;
            for(size_t j=0;j<*count;j++) if(!strcmp(out[j].ssid,net.ssid) && out[j].security==net.security) duplicate=true;
            if(!duplicate && *count<cap) out[(*count)++]=net;
        }
        at+=size;
    }
    return true;
}
int wifi_team_score(const char *ssid,unsigned team) {
    if(!ssid || !team || team>25599) return 0;
    char name[33];
    for(unsigned i=0;i<3;i++) {
        snprintf(name,sizeof name,i==0 ? "FRC-%u":i==1 ? "FRC-AP-%u":"%u",team);
        size_t n=strlen(name);
        if(!strncmp(ssid,name,n) && (ssid[n]==0 || ssid[n]=='-'))
            return (int)(6-i*2)-(ssid[n]=='-');
    }
    return 0;
}
