/* Compile the real native socket functions with only platform declarations faked. */
#define TEST_NATIVE_MAIN
#define main native_app_main
#include "../source/main.c"
#undef main
#include <assert.h>

WifiSetup wifi_setup;
WifiScan wifi_scan;
uint64_t svcGetSystemTick(void) { return 0; }

static int rio;
static struct sockaddr_in local;
static const Controls empty={{0},0,-1};
static void setup(void) {
    safety=(Safety){0}; link_state=(Link){0}; sequence=0;
    sock=socket(AF_INET,SOCK_DGRAM,0); rio=socket(AF_INET,SOCK_DGRAM,0);
    assert(sock>=0 && rio>=0);
    local=(struct sockaddr_in){.sin_family=AF_INET,.sin_addr.s_addr=htonl(INADDR_LOOPBACK)};
    assert(!bind(sock,(struct sockaddr *)&local,sizeof local));
    socklen_t n=sizeof local;
    assert(!getsockname(sock,(struct sockaddr *)&local,&n));
    robot_addr=local;robot_addr.sin_port=0;
    assert(!bind(rio,(struct sockaddr *)&robot_addr,sizeof robot_addr));
    assert(!getsockname(rio,(struct sockaddr *)&robot_addr,&n));
    assert(!fcntl(sock,F_SETFL,O_NONBLOCK));
    configured=true;
}
static void teardown(void) { close(sock);close(rio);sock=-1; }
static void sent(uint64_t now,bool enabled) {
    uint8_t data[DS_PACKET_MAX];
    assert(transmit(&empty,now,false));
    assert(recv(rio,data,sizeof data,MSG_DONTWAIT)>0);
    assert(!!(data[3]&4)==enabled);
}
static void reply(uint16_t seq,uint8_t state,uint8_t trace) {
    uint8_t p[]={(uint8_t)(seq>>8),(uint8_t)seq,1,state,trace,12,128,0};
    assert(sendto(rio,p,sizeof p,0,(struct sockaddr *)&local,sizeof local)==sizeof p);
}
int main(void) {
    setup();sent(100,false);reply(0,0,0x20);
    assert(!receive_status(101) && link_connected(&link_state,101));
    safety.enabled=true;sent(102,true);sent(103,true);
    reply(1,0x80,0x20);reply(2,0,0x20);
    assert(receive_status(104) && safety.estop && !safety.enabled);
    assert(!receive_status(105) && link_state.robot.estop);
    sent(106,false);teardown();

    for(unsigned state=0;state<=8;state+=8) {
        setup();sent(100,false);reply(0,0,0x20);assert(!receive_status(101));
        safety.enabled=true;sent(102,true);sent(103,true);
        reply(1,state,state ? 0x20:0);reply(2,0,0x20);
        assert(receive_status(104) && !safety.enabled && !safety.estop);
        assert(!receive_status(105) && link_state.have_reply && !link_state.robot.code);
        sent(106,false);reply(3,0,0x20);assert(!receive_status(107));
        assert(link_connected(&link_state,107) && !safety.enabled);
        teardown();
    }
    setup();sent(100,false);reply(0,0,0x20);assert(!receive_status(101));
    safety.enabled=true;sent(350,false); /* Last-moment send guard, after a stall. */
    assert(!safety.enabled);teardown();
    setup();sent(100,false);reply(0,0,0x20);assert(!receive_status(101));
    safety.enabled=true;
    assert(!timing_fault(201,101) && safety.enabled);
    assert(timing_fault(202,101) && !safety.enabled && !link_state.have_reply);
    sent(202,false);reply(1,0,0x20);assert(!receive_status(203));
    safety.enabled=true; __atomic_store_n(&lifecycle_event,1,__ATOMIC_RELEASE);
    assert(timing_fault(204,203) && !safety.enabled && !link_state.have_reply);
    teardown();

    setup();sent(100,false);
    /* Correctly framed 2048-byte prefix plus an invalid trailer must not be
     * silently accepted as a truncated datagram by the native recvfrom path. */
    uint8_t oversized[2049]={0,0,1,0,0x20,12,0,0};
    for(size_t at=8;at<2048;at+=255) oversized[at]=254;
    oversized[2048]=255;
    assert(sendto(rio,oversized,sizeof oversized,0,(struct sockaddr *)&local,sizeof local)==sizeof oversized);
    assert(!receive_status(101) && !link_state.have_reply);
    /* Replies from an unrelated address cannot refresh readiness. */
    robot_addr.sin_addr.s_addr=htonl(0x7f000002);
    reply(0,0,0x20);assert(!receive_status(102) && !link_state.have_reply);
    robot_addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    reply(0,0,0x20);assert(!receive_status(103) && link_state.have_reply);
    teardown();
    puts("PASS: native UDP sends, fault/recovery bursts, initialization, stale-send guard, oversized datagrams and source filtering");
}
