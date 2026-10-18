#ifndef N3DS_FEED_H
#define N3DS_FEED_H
#include <stddef.h>
#include <stdint.h>
/* Bounded console history; oldest lines fall off as new text arrives. */
#define FEED_LINES 17
#define FEED_COLUMNS 46
typedef struct {
    char lines[FEED_LINES][FEED_COLUMNS+1];
    unsigned row, column;
    uint8_t frame[65535];
    size_t used, expected;
    uint8_t header[2];
    unsigned header_used;
} Feed;
void feed_clear(Feed *feed);
void feed_reset_stream(Feed *feed);
void feed_text(Feed *feed, const char *text);
void feed_receive(Feed *feed, const uint8_t *data, size_t length);
#endif
