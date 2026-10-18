#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <3ds.h>
#include "wifi_scan.h"
static u32 ipc[32];
static bool complete,fail;
static void put32(unsigned char *p,unsigned v) {for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
u32 *getThreadCommandBuffer(void) {return ipc;}
Result srvGetServiceHandle(Handle *h,const char *name) {assert(!strcmp(name,"nwm::INF"));*h=1;return fail ? -3:0;}
Result svcCreateEvent(Handle *h,unsigned reset) {(void)reset;*h=2;return 0;}
Result svcCloseHandle(Handle h) {assert(h==1 || h==2);return 0;}
Result svcSendSyncRequest(Handle h) {
    assert(h==1 && ipc[0]==0x603c4 && ipc[1]==0x10000);
    assert(ipc[2]==0x00020001 && ipc[3]==0x006e0421);
    assert(ipc[4]==0xffffffff && ipc[5]==0xffff);
    assert(ipc[15]==0 && ipc[16]==0 && ipc[17]==2);
    assert(ipc[18]==0x10000c);
    unsigned char *raw=(void *)ipc[19];
    put32(raw,0x10000);put32(raw+4,12);put32(raw+8,0);ipc[1]=0;return 0;
}
Thread threadCreate(void (*entry)(void *),void *arg,size_t stack,int priority,int core,bool detached) {
    assert(stack>=0x8000 && priority==0x31 && core==-2 && !detached);
    entry(arg);return (void *)1;
}
Result threadJoin(Thread t,uint64_t timeout) {(void)t;(void)timeout;return complete ? 0:-1;}
void threadFree(Thread t) {assert(t==(void *)1);}
int main(void) {
    assert(wifi_scan_start(1));assert(wifi_scan.scanning && !wifi_scan.valid);
    assert(!wifi_scan_start(2));wifi_scan_poll(3);assert(wifi_scan.scanning);
    complete=true;wifi_scan_poll(4);assert(wifi_scan.valid && !wifi_scan.scanning && wifi_scan.count==0);
    assert(wifi_scan_recommended(1234,4)==-1); /* Never fabricate a recommendation. */
    strcpy(wifi_scan.networks[0].ssid,"FRC-12345");wifi_scan.networks[0].security=WIFI_WPA2;wifi_scan.count=1;
    assert(wifi_scan_recommended(1234,4)==-1);
    strcpy(wifi_scan.networks[0].ssid,"FRC-1234");
    assert(wifi_scan_recommended(1234,4)==0);
    wifi_scan.networks[0].security=WIFI_WPA3;assert(wifi_scan_recommended(1234,4)==-1);
    wifi_scan.networks[0].security=WIFI_WPA2;assert(wifi_scan_recommended(1234,30005)==-1);
    assert(wifi_scan_start(6));wifi_scan_cancel();wifi_scan_poll(7);assert(!wifi_scan.valid && !wifi_scan.count);
    fail=true;assert(wifi_scan_start(8));wifi_scan_poll(9);assert(!wifi_scan.valid && wifi_scan.error);
    wifi_scan_exit();puts("PASS: native scan request, single worker, fresh discovered recommendations, cancellation and errors");
}
