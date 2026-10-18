#include "safety.h"
void safety_disable(Safety *s) { s->enabled=false; }
void safety_observe_robot(Safety *s,const RobotStatus *robot) {
    if(robot->estop) s->estop=true;
    if(s->estop || !robot->code) safety_disable(s);
}
void safety_step(Safety *s,const SafetyInput *i) {
    if(i->robot_estop) s->estop=true;
    if(s->estop || i->disable_pressed || !i->connected || !i->code) {
        safety_disable(s);
        return;
    }
    if(i->enable_pressed && i->neutral) s->enabled=true;
}
