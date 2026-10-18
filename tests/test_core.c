#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "protocol.h"
#include "safety.h"
#include "link.h"

static void packets(void) {
    uint8_t out[DS_PACKET_MAX];
    Controls j = {{-127, 0, 127, -1}, 0x341, 315};
    Command c = {65535, 2, true, false, 5};
    const uint8_t expected[] = {255,255,1,6,0,5,12,12,4,129,0,127,255,10,3,65,1,1,59};
    assert(ds_packet(out, sizeof out, &c, &j, NULL) == sizeof expected);
    assert(memcmp(out, expected, sizeof expected) == 0);
    assert(ds_packet(out, sizeof expected - 1, &c, &j, NULL) == 0);
    c.estop = true; ds_packet(out, sizeof out, &c, &j, NULL); assert(out[3] == 0x82);
    c.sequence++; assert(c.sequence == 0);
    Calendar cal = {0x00010203, 4, 5, 6, 7, 9, 126};
    const uint8_t date[] = {11,15,0,1,2,3,4,5,6,7,9,126,4,16,'U','T','C'};
    assert(ds_packet(out, sizeof out, &c, &j, &cal) == 36);
    assert(memcmp(out + 6, date, sizeof date) == 0);
    c.mode = 3; assert(ds_packet(out, sizeof out, &c, &j, NULL) == 0);
    c.mode = 0; c.alliance = 6; assert(ds_packet(out, sizeof out, &c, &j, NULL) == 0);
}
static void status(void) {
    RobotStatus s;
    uint8_t p[] = {0,1,1,4,0x20,12,128,1};
    assert(ds_status(p, sizeof p, &s));
    assert(s.sequence == 1 && s.code && s.enabled && s.voltage == 12.5f && s.request_time);
    p[3] = 0x20; p[4] = 0; assert(ds_status(p, sizeof p, &s)); assert(!s.code);
    p[3] = 0x90; assert(ds_status(p, sizeof p, &s)); assert(s.estop && s.brownout);
    for (size_t n = 0; n < 8; n++) assert(!ds_status(p, n, &s));
    p[2] = 2; assert(!ds_status(p, sizeof p, &s));
    p[2] = 1;
    uint8_t tag[] = {0,1,1,0,0x20,12,0,0,3,1,0};
    assert(!ds_status(tag, sizeof tag, &s));
    tag[8] = 2; assert(ds_status(tag, sizeof tag, &s));
    tag[8] = 0; assert(!ds_status(tag, sizeof tag, &s));
}
static void axes(void) {
    assert(ds_axis(0, 156, 15) == 0 && ds_axis(15,156,15) == 0);
    assert(ds_axis(-15,156,15) == 0 && ds_axis(156,156,15) == 127);
    assert(ds_axis(-156,156,15) == -127 && ds_axis(200,156,15) == 127);
    assert(ds_axis(-200,156,15) == -127 && ds_axis(0,0,0) == 0);
    const int16_t expected[] = {-1,0,90,45,180,-1,135,90,270,315,-1,0,225,270,180,-1};
    for (int bits = 0; bits < 16; bits++)
        assert(ds_pov(bits&1,bits&2,bits&4,bits&8) == expected[bits]);
}
static void safety_tests(void) {
    Safety s={0};
    SafetyInput i={.connected=true,.code=true,.neutral=true};
    safety_step(&s,&i); assert(!s.enabled); /* Connected is not enough. */
    i.enable_pressed=true; safety_step(&s,&i); assert(s.enabled);
    i.enable_pressed=false; i.neutral=false; safety_step(&s,&i); assert(s.enabled); /* Drive normally. */
    i.disable_pressed=true; safety_step(&s,&i); assert(!s.enabled);
    i.disable_pressed=false; i.neutral=true; safety_step(&s,&i); assert(!s.enabled);
    for(int fault=0;fault<4;fault++) {
        s=(Safety){0}; i=(SafetyInput){.connected=true,.code=true,.neutral=true,.enable_pressed=true};
        safety_step(&s,&i); assert(s.enabled);
        i.enable_pressed=false;
        if(fault==0) i.connected=false;
        if(fault==1) i.code=false;
        if(fault==2) i.robot_estop=true;
        if(fault==3) safety_disable(&s); /* HOME/suspend/stall path. */
        safety_step(&s,&i); assert(!s.enabled);
        i.connected=true; i.code=true; i.robot_estop=false;
        safety_step(&s,&i); assert(!s.enabled); /* Recovery cannot enable. */
        i.enable_pressed=true; safety_step(&s,&i); assert(s.enabled==(fault!=2));
    }
    s=(Safety){0}; i=(SafetyInput){.connected=true,.code=true,.enable_pressed=true};
    safety_step(&s,&i); assert(!s.enabled); /* Non-neutral input blocks enable. */
    i.neutral=true; i.connected=false; safety_step(&s,&i); assert(!s.enabled);
    i.connected=true; i.code=false; safety_step(&s,&i); assert(!s.enabled);
    i.code=true; i.disable_pressed=true; safety_step(&s,&i); assert(!s.enabled); /* Stop wins. */
}
static void links(void) {
    Link l = {0}; uint8_t p[] = {255,255,1,0,0x20,12,128,0};
    assert(!link_receive(&l,p,sizeof p,10));
    link_sent(&l,65535,10); assert(link_receive(&l,p,sizeof p,20));
    assert(link_connected(&l,269)); assert(!link_connected(&l,270));
    assert(!link_receive(&l,p,sizeof p,21)); /* Duplicate cannot extend lease. */
    link_sent(&l,0,30); p[0] = 0; p[1] = 0;
    assert(link_receive(&l,p,sizeof p,40)); /* Sequence rollover. */
    link_sent(&l,1,50); p[1] = 1;
    assert(!link_receive(&l,p,sizeof p,300)); /* Reply already too old. */
    link_sent(&l,2,310); p[1] = 2;
    assert(link_receive(&l,p,sizeof p,311));
    link_sent(&l,1,310); p[1] = 1; assert(!link_receive(&l,p,sizeof p,312));
    assert(l.last_rx == 311);
}
int main(void) {
    packets(); status(); axes(); safety_tests(); links();
    puts("PASS: packet vectors, telemetry, inputs, safety interlocks, freshness, rollover");
}
