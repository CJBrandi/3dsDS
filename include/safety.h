#ifndef N3DS_SAFETY_H
#define N3DS_SAFETY_H
#include "protocol.h"
typedef struct {
    bool enabled, estop, arming, must_release;
    uint64_t arm_since;
} Safety;
typedef struct {
    bool connected, code, robot_estop, deadman, start, disable, emergency, neutral;
} SafetyInput;
void safety_disable(Safety *s);
void safety_step(Safety *s, const SafetyInput *i, uint64_t now);
#endif
