#include "safety.h"
void safety_disable(Safety *s) {
    s->enabled = false; s->arming = false; s->must_release = true;
}
void safety_step(Safety *s, const SafetyInput *i, uint64_t now) {
    if (i->emergency || i->robot_estop) s->estop = true;
    if (s->estop || i->disable || !i->connected || !i->code || !i->deadman) {
        safety_disable(s);
        /* A new press is required after a fault, even if START stayed held. */
        if (!i->start) s->must_release = false;
        return;
    }
    if (s->enabled) {
        if (!i->start) s->must_release = false;
        if (i->start && !s->must_release) safety_disable(s);
        return;
    }
    if (!i->start) {
        s->must_release = false; s->arming = false; return;
    }
    if (s->must_release || !i->neutral) { s->arming = false; return; }
    if (!s->arming) { s->arming = true; s->arm_since = now; }
    if (now - s->arm_since >= 1000) {
        s->enabled = true; s->arming = false;
        /* Don't interpret the still-held arming press as disable next frame. */
        s->must_release = true;
    }
}
