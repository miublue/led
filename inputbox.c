#include <string.h>
#include <ctype.h>
#include <ncurses.h>
#include "inputbox.h"

static inline void input_insert_char(inputbox_t *ib, int c) {
    ib->text_sz++;
    memmove(ib->text+ib->pos+1, ib->text+ib->pos, ib->text_sz-ib->pos);
    ib->text[ib->pos++] = c;
}

static inline void input_remove_char(inputbox_t *ib) {
    memmove(ib->text+ib->pos, ib->text+ib->pos+1, ib->text_sz-ib->pos);
    ib->text_sz--;
}

void input_update(inputbox_t *ib, int key) {
    switch (key) {
    case KEY_LEFT:
        if (ib->pos > 0) --ib->pos;
        break;
    case KEY_RIGHT:
        if (ib->pos < ib->text_sz) ++ib->pos;
        break;
    case KEY_UP:
    case KEY_HOME:
        ib->pos = 0;
        break;
    case KEY_DOWN:
    case KEY_END:
        ib->pos = ib->text_sz;
        break;
    case KEY_DC:
        input_remove_char(ib);
        break;
    case KEY_BACKSPACE:
        if (ib->pos == 0) break;
        --ib->pos;
        input_remove_char(ib);
        break;
    default:
        if (isprint(key)) input_insert_char(ib, key);
        break;
    }
}

void input_render(inputbox_t *ib, int x, int y, int w) {
    char text[INPUTBOX_TEXT_SIZE] = {0};
    memset(text, ' ', sizeof(text));
    memcpy(text, ib->text, MIN(ib->text_sz, w));
    mvprintw(y, x, "%.*s", w, text);
    attron(A_REVERSE);
    mvprintw(y, x+ib->pos, "%c", isprint(ib->text[ib->pos])? ib->text[ib->pos] : ' ');
    attroff(A_REVERSE);
}

void input_reset(inputbox_t *ib) {
    ib->text_sz = ib->pos = 0;
    memset(ib->text, 0, sizeof(ib->text));
}
