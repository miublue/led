#ifndef __INPUTBOX_H
#define __INPUTBOX_H

#include <inttypes.h>

#define MIN(a, b) ((a) < (b)? (a) : (b))
#define MAX(a, b) ((a) > (b)? (a) : (b))
#define INPUTBOX_TEXT_SIZE 1024

typedef struct inputbox_t {
    char text[INPUTBOX_TEXT_SIZE];
    uint8_t text_sz, pos;
} inputbox_t;

void input_update(inputbox_t *ib, int key);
void input_render(inputbox_t *ib, int x, int y, int w);
void input_reset(inputbox_t *ib);
void input_set(inputbox_t *ib, char *text, int sz);

#endif
