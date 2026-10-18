#ifndef N3DS_SAFETY_H
#define N3DS_SAFETY_H
#include "protocol.h"
typedef struct { bool enabled, estop; } Safety;
typedef struct {
    bool connected, code, robot_estop, enable_pressed, disable_pressed, neutral;
} SafetyInput;
void safety_disable(Safety *s);
/* Enable/disable inputs are touch-down edges, never held button states. */
void safety_step(Safety *s, const SafetyInput *i);
#endif
