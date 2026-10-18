#ifndef N3DS_LINK_H
#define N3DS_LINK_H
#include "protocol.h"
typedef struct {
    uint16_t seq[32];
    uint64_t sent[32];
    bool valid[32], have_reply;
    uint16_t last_reply;
    uint64_t last_rx;
    RobotStatus robot;
} Link;
void link_sent(Link *l, uint16_t sequence, uint64_t now);
bool link_receive(Link *l, const uint8_t *data, size_t length, uint64_t now);
bool link_connected(const Link *l, uint64_t now);
#endif
