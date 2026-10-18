#include "protocol.h"

static void put16(uint8_t *p, uint16_t n) { p[0] = n >> 8; p[1] = n; }
static uint16_t get16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

size_t ds_packet(uint8_t *p, size_t cap, const Command *c,
                 const Controls *j, const Calendar *t) {
    const size_t needed = 19 + (t ? 17 : 0);
    if (!p || !c || !j || cap < needed || c->mode > 2 || c->alliance > 5) return 0;
    put16(p, c->sequence);
    p[2] = 1;
    p[3] = c->mode | (c->enabled && !c->estop ? 4 : 0) | (c->estop ? 0x80 : 0);
    p[4] = 0; /* No reboot/restart requests. */
    p[5] = c->alliance;
    size_t n = 6;
    if (t) {
        p[n++] = 11; p[n++] = 0x0f;
        p[n++] = t->usec >> 24; p[n++] = t->usec >> 16;
        p[n++] = t->usec >> 8; p[n++] = t->usec;
        p[n++] = t->sec; p[n++] = t->min; p[n++] = t->hour;
        p[n++] = t->day; p[n++] = t->month; p[n++] = t->year;
        p[n++] = 4; p[n++] = 0x10;
        p[n++] = 'U'; p[n++] = 'T'; p[n++] = 'C';
    }
    p[n++] = 12; p[n++] = 0x0c; /* Size includes ID, excludes size byte. */
    p[n++] = 4;
    for (int i = 0; i < 4; ++i) p[n++] = (uint8_t)j->axes[i];
    p[n++] = 10; put16(p+n,j->buttons & 0x03ff); n+=2;
    p[n++] = 1; put16(p + n, (uint16_t)j->pov); n += 2;
    return n;
}

bool ds_status(const uint8_t *p, size_t n, RobotStatus *out) {
    if (!p || !out || n < 8 || p[2] != 1 || (p[3] & 3) == 3) return false;
    /* Refuse truncated/zero-length extension tags. Unknown complete tags are fine. */
    for (size_t i = 8; i < n;) {
        size_t length = p[i];
        if (length < 1 || length > n - i - 1) return false;
        i += length + 1;
    }
    RobotStatus s = {0};
    s.sequence = get16(p);
    s.code = (p[4] & 0x20) != 0; /* Trace, not the control/status byte. */
    s.enabled = (p[3] & 4) != 0;
    s.estop = (p[3] & 0x80) != 0;
    s.brownout = (p[3] & 0x10) != 0;
    s.voltage = p[5] + p[6] / 256.0f;
    s.request_time = p[7] != 0;
    *out = s;
    return true;
}

int8_t ds_axis(int raw, int scale, int dead) {
    if (scale <= dead || dead < 0) return 0;
    if (raw > scale) raw = scale;
    if (raw < -scale) raw = -scale;
    if (raw >= -dead && raw <= dead) return 0;
    int magnitude = raw < 0 ? -raw : raw;
    int result = (magnitude - dead) * 127 / (scale - dead);
    return (int8_t)(raw < 0 ? -result : result);
}
int16_t ds_pov(bool u, bool r, bool d, bool l) {
    int x = (int)r - (int)l, y = (int)u - (int)d;
    if (!x && !y) return -1;
    if (!x) return y > 0 ? 0 : 180;
    if (!y) return x > 0 ? 90 : 270;
    return y > 0 ? (x > 0 ? 45 : 315) : (x > 0 ? 135 : 225);
}
