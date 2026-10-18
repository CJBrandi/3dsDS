/* AC service IPC commands: https://www.3dbrew.org/wiki/AC_Services
 * Profile lengths: https://www.3dbrew.org/wiki/Config_Savegame#WiFi_Slot_Structure
 * Credentials are kept in the 3DS system profile, never the app's SD config. */
#include <3ds.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <stdio.h>
#include <string.h>
#include "wifi.h"
WifiSetup wifi_setup;
static Handle event;
static enum { PHASE_IDLE, PHASE_CLOSE, PHASE_CONNECT } phase;
static uint64_t deadline;
static bool cancel_after_close;
static Result command_scalar(unsigned id,uint32_t value) {
    u32 *c=getThreadCommandBuffer(); c[0]=IPC_MakeHeader(id,1,0); c[1]=value;
    Result r=svcSendSyncRequest(*acGetSessionHandle());
    return R_FAILED(r) ? r:(Result)c[1];
}
static Result command_empty(unsigned id) {
    u32 *c=getThreadCommandBuffer(); c[0]=IPC_MakeHeader(id,0,0);
    Result r=svcSendSyncRequest(*acGetSessionHandle());
    return R_FAILED(r) ? r:(Result)c[1];
}
static Result command_buffer(unsigned id,const void *buffer,size_t n) {
    u32 *c=getThreadCommandBuffer(); c[0]=IPC_MakeHeader(id,0,2);
    c[1]=IPC_Desc_StaticBuffer(n,0); c[2]=(u32)buffer;
    Result r=svcSendSyncRequest(*acGetSessionHandle());
    return R_FAILED(r) ? r:(Result)c[1];
}
static Result command_process(unsigned id,bool with_event) {
    u32 *c=getThreadCommandBuffer(); c[0]=IPC_MakeHeader(id,0,with_event ? 4:2);
    c[1]=IPC_Desc_CurProcessId(); c[2]=0;
    if(with_event) { c[3]=IPC_Desc_SharedHandles(1); c[4]=event; }
    Result r=svcSendSyncRequest(*acGetSessionHandle());
    return R_FAILED(r) ? r:(Result)c[1];
}
static void finish(const char *status,Result result) {
    wifi_setup.busy=false; phase=PHASE_IDLE;
    if(event) { svcCloseHandle(event); event=0; }
    wifi_setup.error=(uint32_t)result;
    if(R_FAILED(result)) snprintf(wifi_setup.status,sizeof wifi_setup.status,"%s (%08lX)",status,(unsigned long)(uint32_t)result);
    else snprintf(wifi_setup.status,sizeof wifi_setup.status,"%s",status);
}
void wifi_setup_refresh_profiles(void) {
    wifi_setup.available=true;
    for(unsigned i=0;i<3;i++) {
        memset(wifi_setup.profiles[i],0,33);
        Result r=ACI_LoadNetworkSetting(i);
        if(R_SUCCEEDED(r)) r=ACI_GetNetworkWirelessEssidSecuritySsid(wifi_setup.profiles[i]);
        wifi_setup.profiles[i][32]=0;
        if(R_FAILED(r)) { wifi_setup.available=false; wifi_setup.error=(uint32_t)r; }
    }
}
void wifi_setup_init(void) {
    memset(&wifi_setup,0,sizeof wifi_setup); wifi_setup.slot=2;
    wifi_setup.security=wifi_setup.saved_security=WIFI_UNSUPPORTED;
    wifi_setup_refresh_profiles();
    for(unsigned i=0;i<3;i++) if(!wifi_setup.profiles[i][0]) { wifi_setup.slot=i; break; }
    snprintf(wifi_setup.status,sizeof wifi_setup.status,wifi_setup.available ?
        "Scan and select a network to join.":"Wi-Fi setup requires ac:i access (CIA build).");
}
bool wifi_setup_select(const char *ssid,WifiSecurity security) {
    if(wifi_setup.busy || !ssid || !ssid[0] || strlen(ssid)>32 ||
       (security!=WIFI_OPEN && security!=WIFI_WPA2) || wifi_setup.slot>=3) return false;
    bool same=!strcmp(wifi_setup.ssid,ssid) && wifi_setup.security==security;
    if(!same) {
        wifi_wipe(wifi_setup.password,sizeof wifi_setup.password);
        wifi_setup.password_saved=!strcmp(wifi_setup.profiles[wifi_setup.slot],ssid) &&
            wifi_setup.saved_security==security;
    }
    memmove(wifi_setup.ssid,ssid,strlen(ssid)+1); wifi_setup.security=security;
    return true;
}
static int hex(char c) { return c<='9' ? c-'0':(c|32)-'a'+10; }
static bool derive_key(unsigned char key[32]) {
    const char *pass=wifi_setup.password;
    if(strlen(pass)==64) {
        for(int i=0;i<32;i++) key[i]=(unsigned char)((hex(pass[i*2])<<4)|hex(pass[i*2+1]));
        return true;
    }
    mbedtls_md_context_t md;
    mbedtls_md_init(&md);
    int r=mbedtls_md_setup(&md,mbedtls_md_info_from_type(MBEDTLS_MD_SHA1),1);
    if(!r) r=mbedtls_pkcs5_pbkdf2_hmac(&md,(const unsigned char *)pass,strlen(pass),
        (const unsigned char *)wifi_setup.ssid,strlen(wifi_setup.ssid),4096,32,key);
    mbedtls_md_free(&md);
    return r==0;
}
static Result save_profile(void) {
    char ssid[32]={0},pass[64]={0}; unsigned char key[32]={0};
    memcpy(ssid,wifi_setup.ssid,strlen(wifi_setup.ssid));
    if(wifi_setup.security==WIFI_WPA2) {
        memcpy(pass,wifi_setup.password,strlen(wifi_setup.password));
        if(!derive_key(key)) { wifi_wipe(pass,sizeof pass); wifi_wipe(key,sizeof key); return -1; }
    }
    Result r=ACI_LoadNetworkSetting(wifi_setup.slot);
    /* Stage the complete profile in AC's working buffer before committing. */
#define STEP(call) do { if(R_SUCCEEDED(r)) r=(call); } while(0)
    STEP(command_scalar(0x40c,1)); /* Wireless enabled. */
    STEP(command_scalar(0x40e,1)); /* Primary SSID enabled/editable. */
    STEP(command_buffer(0x410,ssid,sizeof ssid));
    STEP(command_scalar(0x412,strlen(wifi_setup.ssid)));
    STEP(command_scalar(0x414,wifi_setup.security==WIFI_OPEN ? AC_OPEN:AC_WPA2_AES));
    STEP(command_buffer(0x416,pass,sizeof pass));
    STEP(command_buffer(0x418,key,sizeof key));
    STEP(command_scalar(0x41a,0)); /* No WPS/multi-SSID fallback. */
    STEP(command_scalar(0x448,1)); /* DHCP. */
    STEP(command_scalar(0x44a,1)); /* Automatic DNS. */
    STEP(command_scalar(0x456,0)); /* Forget the old AP's cached BSSID/channel. */
    STEP(command_scalar(0x460,0)); /* No stale proxy settings. */
    STEP(command_scalar(0x46e,1400));
    STEP(command_scalar(0x402,wifi_setup.slot)); /* Update selected profile. */
    STEP(command_empty(0x404)); /* Flush system settings. */
#undef STEP
    wifi_wipe(pass,sizeof pass); wifi_wipe(key,sizeof key);
    if(R_FAILED(r)) { ACI_LoadNetworkSetting(wifi_setup.slot); return r; }
    memmove(wifi_setup.profiles[wifi_setup.slot],wifi_setup.ssid,33);
    wifi_setup.password_saved=true;
    wifi_setup.saved_security=wifi_setup.security;
    return 0;
}
static Result start_connect(uint64_t now) {
    acuConfig config; Result r=ACU_CreateDefaultConfig(&config);
    if(R_SUCCEEDED(r)) r=ACU_SetNetworkArea(&config,2);
    if(R_SUCCEEDED(r)) r=ACU_SetAllowApType(&config,1u<<(wifi_setup.slot+1));
    if(R_SUCCEEDED(r)) { svcClearEvent(event); r=ACU_ConnectAsync(&config,event); }
    if(R_SUCCEEDED(r)) { phase=PHASE_CONNECT; deadline=now+30000;
        snprintf(wifi_setup.status,sizeof wifi_setup.status,"Connecting to %.32s...",wifi_setup.ssid); }
    return r;
}
bool wifi_setup_connect(uint64_t now) {
    if(wifi_setup.busy || !wifi_setup.available || wifi_setup.slot>=3) return false;
    if(wifi_setup.security!=WIFI_OPEN && wifi_setup.security!=WIFI_WPA2) {
        snprintf(wifi_setup.status,sizeof wifi_setup.status,"Scan and select a supported network first."); return false;
    }
    if(!wifi_setup.ssid[0] || strlen(wifi_setup.ssid)>32) {
        snprintf(wifi_setup.status,sizeof wifi_setup.status,"Enter a network name (1-32 bytes)."); return false;
    }
    bool reuse=wifi_setup.password_saved && wifi_setup.saved_security==wifi_setup.security &&
        !strcmp(wifi_setup.profiles[wifi_setup.slot],wifi_setup.ssid);
    if(!reuse && !wifi_credentials_valid(wifi_setup.ssid,wifi_setup.password,wifi_setup.security)) {
        snprintf(wifi_setup.status,sizeof wifi_setup.status,"WPA2 needs 8-63 characters or 64 hex digits."); return false;
    }
    u32 connected=0;
    Result r=ACU_GetWifiStatus(&connected);
    if(R_FAILED(r)) { finish("Could not read Wi-Fi status",r); return false; }
    r=reuse ? 0:save_profile();
    if(R_FAILED(r)) { finish("Could not save Wi-Fi profile",r); return false; }
    r=svcCreateEvent(&event,RESET_ONESHOT);
    if(R_FAILED(r)) { finish("Could not start connection",r); return false; }
    wifi_setup.busy=true;
    cancel_after_close=false;
    if(connected) {
        r=command_process(0x08,true);
        if(R_SUCCEEDED(r)) {
            phase=PHASE_CLOSE; deadline=now+5000;
            snprintf(wifi_setup.status,sizeof wifi_setup.status,"Switching Wi-Fi networks...");
        }
    } else r=start_connect(now);
    if(R_FAILED(r)) { finish("Connection failed",r); return false; }
    return true;
}
void wifi_setup_poll(uint64_t now) {
    if(!wifi_setup.busy) return;
    if(now>=deadline) {
        if(phase==PHASE_CONNECT) command_process(0x07,false);
        finish("Connection timed out. Check Wi-Fi and password.",-1); return;
    }
    if(R_FAILED(svcWaitSynchronization(event,0))) return;
    Result r=command_process(phase==PHASE_CLOSE ? 0x09:0x05,false);
    if(phase==PHASE_CLOSE) {
        if(cancel_after_close) { finish("Connection cancelled.",r); return; }
        if(R_SUCCEEDED(r)) r=start_connect(now);
        if(R_FAILED(r)) finish("Could not switch Wi-Fi",r);
        return;
    }
    char ssid[33]={0}; u32 connected=0;
    if(R_SUCCEEDED(r)) r=ACU_GetWifiStatus(&connected);
    if(R_SUCCEEDED(r)) r=ACU_GetSSID(ssid);
    ssid[32]=0;
    if(R_SUCCEEDED(r) && connected && !strcmp(ssid,wifi_setup.ssid)) finish("Wi-Fi connected. Waiting for robot.",0);
    else finish("Connection failed; check network/password",R_FAILED(r) ? r:-1);
}
void wifi_setup_cancel(void) {
    /* CloseAsync has no cancel command. Drain its result before another join. */
    if(phase==PHASE_CLOSE) {
        cancel_after_close=true;
        snprintf(wifi_setup.status,sizeof wifi_setup.status,"Cancelling connection; waiting for Wi-Fi close...");
        return;
    }
    if(phase==PHASE_CONNECT) command_process(0x07,false);
    finish("Connection cancelled.",0);
}
void wifi_setup_exit(void) {
    wifi_setup_cancel();
    if(phase==PHASE_CLOSE) finish("Connection cancelled.",0);
    wifi_wipe(wifi_setup.password,sizeof wifi_setup.password);
}
