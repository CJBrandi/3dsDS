#include "wifi_settings.h"
#include <string.h>
static bool printable_ascii(const char *s) {
    for(;*s;s++) if((unsigned char)*s<32 || (unsigned char)*s>126) return false;
    return true;
}
bool wifi_credentials_valid(const char *ssid,const char *password,WifiSecurity mode) {
    if(!ssid || !password || !ssid[0] || strlen(ssid)>32) return false;
    if(mode==WIFI_OPEN) return true;
    if(mode!=WIFI_WPA2) return false;
    size_t n=strlen(password);
    if(n>=8 && n<=63) return printable_ascii(password);
    if(n!=64) return false;
    for(size_t i=0;i<n;i++) {
        char c=password[i];
        if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'))) return false;
    }
    return true;
}
void wifi_wipe(void *p,size_t n) { volatile uint8_t *v=p; while(n--) *v++=0; }
