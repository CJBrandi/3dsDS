#ifndef N3DS_PROTOCOL_H
#define N3DS_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define DS_PORT 1110
#define DS_STATUS_PORT 1150
#define DS_PACKET_MAX 64
#define DS_TIMEOUT_MS 250
#define DS_PERIOD_MS 20

typedef struct { int8_t axes[4]; uint16_t buttons; int16_t pov; } Controls;
typedef struct {
    uint16_t sequence;
    uint8_t mode;
    bool enabled, estop;
    uint8_t alliance;
} Command;
typedef struct {
    uint16_t sequence;
    bool code, enabled, estop, brownout, request_time;
    float voltage;
} RobotStatus;
/* UTC calendar fields use struct tm conventions for month/year. */
typedef struct { uint32_t usec; uint8_t sec, min, hour, day, month, year; } Calendar;
size_t ds_packet(uint8_t *out, size_t capacity, const Command *cmd,
                 const Controls *controls, const Calendar *calendar);
bool ds_status(const uint8_t *data, size_t length, RobotStatus *out);
int8_t ds_axis(int raw, int full_scale, int deadzone);
int16_t ds_pov(bool up, bool right, bool down, bool left);
#endif
