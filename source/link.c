#include "link.h"
void link_sent(Link *l, uint16_t seq, uint64_t now) {
    unsigned slot = seq % 32;
    l->seq[slot] = seq; l->sent[slot] = now; l->valid[slot] = true;
}
bool link_receive(Link *l, const uint8_t *data, size_t n, uint64_t now) {
    RobotStatus r;
    if (!ds_status(data, n, &r)) return false;
    unsigned slot = r.sequence % 32;
    if (!l->valid[slot] || l->seq[slot] != r.sequence ||
        now < l->sent[slot] || now - l->sent[slot] >= DS_TIMEOUT_MS) return false;
    uint16_t delta = (uint16_t)(r.sequence - l->last_reply);
    if (l->have_reply && (delta == 0 || delta >= 0x8000)) return false;
    l->valid[slot] = false;
    l->have_reply = true; l->last_reply = r.sequence; l->last_rx = now; l->robot = r;
    return true;
}
bool link_connected(const Link *l, uint64_t now) {
    return l->have_reply && now >= l->last_rx && now - l->last_rx < DS_TIMEOUT_MS;
}
