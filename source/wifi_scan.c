/* Infrastructure scan IPC and reply layout:
 * https://docs.mikage.app/NWMINFRecvBeaconBroadcastData/
 * https://docs.mikage.app/NWM_Services/#beacondatareply-structure
 * General ScanInputStruct parameters match devkitPro's documented NWM interface. */
#include <3ds.h>
#include <string.h>
#include "wifi_scan.h"
WifiScan wifi_scan;
static Thread worker;
static uint8_t raw[0x10000] __attribute__((aligned(0x1000)));
static Result worker_result;
static uint32_t discard;
static void scan_worker(void *arg) {
    (void)arg;
    Handle session=0,event=0;
    Result r=srvGetServiceHandle(&session,"nwm::INF");
    if(R_SUCCEEDED(r)) r=svcCreateEvent(&event,RESET_ONESHOT);
    if(R_SUCCEEDED(r)) {
        uint8_t input[0x34]={1,0,2,0,0x21,4,0x6e,0};
        memset(input+8,0xff,6);
        u32 *c=getThreadCommandBuffer();
        c[0]=IPC_MakeHeader(0x6,15,4);c[1]=sizeof raw;
        for(unsigned i=0;i<13;i++) c[2+i]=(u32)input[i*4]|((u32)input[i*4+1]<<8)|((u32)input[i*4+2]<<16)|((u32)input[i*4+3]<<24);
        c[15]=0;
        c[16]=IPC_Desc_SharedHandles(1);c[17]=event;
        c[18]=IPC_Desc_Buffer(sizeof raw,IPC_BUFFER_W);c[19]=(u32)raw;
        r=svcSendSyncRequest(session);
        if(R_SUCCEEDED(r)) r=(Result)c[1];
    }
    if(event) svcCloseHandle(event);
    if(session) svcCloseHandle(session);
    worker_result=r;
}
bool wifi_scan_start(uint64_t now) {
    (void)now;
    if(worker) return false;
    wifi_scan.valid=false;wifi_scan.count=0;wifi_scan.error=0;
    memset(raw,0,sizeof raw);__atomic_store_n(&discard,0,__ATOMIC_RELEASE);
    worker=threadCreate(scan_worker,NULL,0x8000,0x31,-2,false);
    wifi_scan.scanning=worker!=NULL;
    if(!worker) wifi_scan.error=(uint32_t)-1;
    return worker!=NULL;
}
void wifi_scan_poll(uint64_t now) {
    if(!worker || R_FAILED(threadJoin(worker,0))) return;
    threadFree(worker);worker=NULL;wifi_scan.scanning=false;
    if(__atomic_load_n(&discard,__ATOMIC_ACQUIRE)) { wifi_scan.valid=false;wifi_scan.count=0;return; }
    wifi_scan.error=(uint32_t)worker_result;
    if(R_SUCCEEDED(worker_result)) {
        wifi_scan.valid=wifi_parse_beacons(raw,sizeof raw,wifi_scan.networks,WIFI_MAX_NETWORKS,&wifi_scan.count);
        if(!wifi_scan.valid) { wifi_scan.count=0;wifi_scan.error=(uint32_t)-2; }
        else wifi_scan.scanned_at=now;
    }
}
void wifi_scan_cancel(void) {
    __atomic_store_n(&discard,1,__ATOMIC_RELEASE);
    wifi_scan.valid=false;wifi_scan.count=0;
}
void wifi_scan_exit(void) {
    wifi_scan_cancel();
    if(worker && R_SUCCEEDED(threadJoin(worker,2000000000ULL))) { threadFree(worker);worker=NULL; }
    /* If the service has not returned, process teardown owns the live worker. */
}
int wifi_scan_recommended(unsigned team,uint64_t now) {
    if(!wifi_scan.valid || now<wifi_scan.scanned_at || now-wifi_scan.scanned_at>WIFI_SCAN_FRESH_MS) return -1;
    int best=-1,score=0;
    for(size_t i=0;i<wifi_scan.count;i++) {
        int candidate=wifi_team_score(wifi_scan.networks[i].ssid,team);
        if(wifi_security_supported(wifi_scan.networks[i].security) && candidate>score) {best=(int)i;score=candidate;}
    }
    return best;
}
