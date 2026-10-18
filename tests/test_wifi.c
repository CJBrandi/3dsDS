#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <3ds.h>
#include "wifi.h"
static u32 ipc[16];
static Handle service=9;
static bool signaled,associated,hang;
static unsigned fail_command,flushes,connects,cancels,mask;
static char stage_ssid[32],stage_pass[64],current_ssid[33];
static unsigned char stage_key[32];
static unsigned ssid_length,security;
static Result status_result,ssid_result,connect_result;
static bool close_hang;
u32 *getThreadCommandBuffer(void) { return ipc; }
Handle *acGetSessionHandle(void) { return &service; }
Result svcSendSyncRequest(Handle h) {
    assert(h==service);
    unsigned id=ipc[0]>>16;
    if(id==fail_command) {ipc[1]=(u32)-17;return 0;}
    if(id==0x410) {assert((ipc[1]>>14)==32);memcpy(stage_ssid,(void*)ipc[2],32);}
    if(id==0x416) {assert((ipc[1]>>14)==64);memcpy(stage_pass,(void*)ipc[2],64);}
    if(id==0x418) {assert((ipc[1]>>14)==32);memcpy(stage_key,(void*)ipc[2],32);}
    if(id==0x412) ssid_length=ipc[1];
    if(id==0x414) security=ipc[1];
    if(id==0x404) flushes++;
    if(id==0x08) {
        assert(ipc[0]==0x80004 && ipc[1]==0x20 && ipc[3]==0 && ipc[4]==1);
        if(!close_hang) {associated=false;signaled=true;}
    }
    if(id==0x05) {assert(ipc[0]==0x50002 && ipc[1]==0x20);ipc[1]=(u32)connect_result;return 0;}
    if(id==0x07) cancels++;
    ipc[1]=0;return 0;
}
Result svcCloseHandle(Handle h) {(void)h;return 0;}
Result svcCreateEvent(Handle *h,unsigned reset) {(void)reset;*h=1;signaled=false;return 0;}
Result svcClearEvent(Handle h) {(void)h;signaled=false;return 0;}
Result svcWaitSynchronization(Handle h,int64_t timeout) {(void)h;assert(timeout==0);return signaled ? 0:-1;}
Result ACI_LoadNetworkSetting(unsigned slot) {assert(slot<3);return 0;}
Result ACI_GetNetworkWirelessEssidSecuritySsid(void *ssid) {memset(ssid,0,32);return 0;}
Result ACU_CreateDefaultConfig(acuConfig *c) {memset(c,0,sizeof *c);return 0;}
Result ACU_SetNetworkArea(acuConfig *c,unsigned area) {(void)c;assert(area==2);return 0;}
Result ACU_SetAllowApType(acuConfig *c,unsigned m) {c->slot_mask=m;return 0;}
Result ACU_ConnectAsync(const acuConfig *c,Handle e) {
    (void)e;mask=c->slot_mask;connects++;
    if(!hang) {associated=true;signaled=true;memcpy(current_ssid,wifi_setup.ssid,33);}
    return 0;
}
Result ACU_GetWifiStatus(u32 *out) {*out=associated ? 2:0;return status_result;}
Result ACU_GetSSID(char *out) {memcpy(out,current_ssid,32);return ssid_result;}
static void reset(void) {
    wifi_setup_exit();
    associated=false;hang=false;fail_command=0;flushes=connects=cancels=0;
    status_result=ssid_result=connect_result=0;close_hang=false;
    memset(stage_key,0xff,32);memset(stage_pass,0xff,64);
    wifi_setup_init();
    assert(wifi_setup_select("IEEE",WIFI_WPA2));strcpy(wifi_setup.password,"password");
}
static void settings(void) {
    assert(wifi_credentials_valid("IEEE","password",WIFI_WPA2));
    assert(!wifi_credentials_valid("IEEE","short",WIFI_WPA2));
    assert(!wifi_credentials_valid("","password",WIFI_WPA2));
    assert(wifi_credentials_valid("IEEE","",WIFI_OPEN));
    assert(!wifi_credentials_valid("IEEE","password\n",WIFI_WPA2));
    assert(!wifi_credentials_valid("IEEE","password",(WifiSecurity)99));
}
int main(void) {
    settings(); reset(); wifi_setup.slot=2;
    assert(wifi_setup_connect(100));assert(wifi_setup.busy);
    assert(flushes==1 && connects==1 && mask==(1u<<3));
    assert(ssid_length==4 && security==7 && !memcmp(stage_ssid,"IEEE",4));
    assert(!memcmp(stage_pass,"password",8));
    const unsigned char expected[32]={0xf4,0x2c,0x6f,0xc5,0x2d,0xf0,0xeb,0xef,0x9e,0xbb,0x4b,0x90,0xb3,0x8a,0x5f,0x90,0x2e,0x83,0xfe,0x1b,0x13,0x5a,0x70,0xe2,0x3a,0xed,0x76,0x2e,0x97,0x10,0xa1,0x2e};
    assert(!memcmp(stage_key,expected,32));
    assert(wifi_setup.password_saved && !strcmp(wifi_setup.password,"password"));
    wifi_setup_poll(110);assert(!wifi_setup.busy && !wifi_setup.error);
    assert(wifi_setup_connect(120)); /* Reconnect saved profile, no password required. */
    wifi_setup_poll(125);wifi_setup_poll(130);
    assert(!wifi_setup.busy && flushes==1 && connects==2);
    reset();fail_command=0x418;
    assert(!wifi_setup_connect(0));assert(flushes==0 && connects==0 && !wifi_setup.busy);
    assert(!wifi_setup.password_saved); /* Never claim a failed write was saved. */
    reset();hang=true;
    assert(wifi_setup_connect(100));wifi_setup_poll(30101);
    assert(!wifi_setup.busy && cancels==1);
    reset();wifi_setup.security=WIFI_OPEN;wifi_setup.password[0]=0;
    assert(wifi_setup_connect(0));
    for(unsigned i=0;i<32;i++) assert(stage_key[i]==0);
    for(unsigned i=0;i<64;i++) assert(stage_pass[i]==0);
    assert(security==0);wifi_setup_cancel();assert(!wifi_setup.busy && cancels==1);
    reset(); status_result=-8;
    assert(!wifi_setup_connect(0) && !flushes && wifi_setup.error);
    reset(); assert(wifi_setup_connect(0)); connect_result=-9; wifi_setup_poll(1);
    assert(!wifi_setup.busy && wifi_setup.error); /* SSID alone cannot override service failure. */
    reset(); assert(wifi_setup_connect(0)); ssid_result=-10; wifi_setup_poll(1);
    assert(!wifi_setup.busy && wifi_setup.error);
    reset(); associated=true; close_hang=true;
    assert(wifi_setup_connect(0)); wifi_setup_cancel();
    assert(wifi_setup.busy && !wifi_setup_connect(1));
    associated=false;signaled=true;wifi_setup_poll(2);
    assert(!wifi_setup.busy && !connects); /* Cancel during close must never reconnect. */
    reset(); associated=true;close_hang=true; assert(wifi_setup_connect(0));
    wifi_setup_cancel();wifi_setup_poll(5000);assert(!wifi_setup.busy && wifi_setup.error);
    reset(); assert(wifi_setup_connect(0));wifi_setup_poll(1);
    assert(wifi_setup.saved_security==WIFI_WPA2);
    wifi_setup.password_saved=false;strcpy(wifi_setup.password,"new password");
    assert(wifi_setup_select("IEEE",WIFI_WPA2) && !wifi_setup.password_saved);
    assert(!strcmp(wifi_setup.password,"new password"));
    assert(wifi_setup_connect(2) && flushes==2);wifi_setup_poll(3);wifi_setup_poll(4);
    assert(!memcmp(stage_pass,"new password",12));
    assert(wifi_setup_select("Other",WIFI_OPEN) && !wifi_setup.password[0]);
    assert(wifi_setup.saved_security==WIFI_WPA2 && !wifi_setup.password_saved);
    assert(wifi_setup_select("IEEE",WIFI_WPA2) && wifi_setup.password_saved);
    wifi_setup.security=WIFI_UNSUPPORTED;
    assert(!wifi_setup_connect(5)); /* Saved credentials cannot bypass detected security. */
    wifi_setup.slot=3;assert(!wifi_setup_connect(5));
    wifi_setup_exit();
    puts("PASS: Wi-Fi credentials, PSK vector, profile staging, reconnect, timeout and cancellation");
}
