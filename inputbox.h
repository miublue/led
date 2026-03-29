#ifndef __INPUTBOX_H
#define __INPUTBOX_H

#include <inttypes.h>

#ifndef INPUTBOX_TEXT_SIZE
#define INPUTBOX_TEXT_SIZE 1024
#endif

struct inputbox {
    char text[INPUTBOX_TEXT_SIZE];
    uint16_t text_sz, pos;
};

void input_update(struct inputbox *ib, int key);
void input_render(struct inputbox *ib, int x, int y, int w, int attr);
void input_reset(struct inputbox *ib);
void input_set(struct inputbox *ib, char *text, int sz);

#ifdef INPUTBOX_IMPL

#include <string.h>
#include <ctype.h>
#include <ncurses.h>
#include "inputbox.h"

static inline void _input_insert_char(struct inputbox *ib, int c) {
    ib->text_sz++;
    memmove(ib->text+ib->pos+1, ib->text+ib->pos, ib->text_sz-ib->pos);
    ib->text[ib->pos++] = c;
}

static inline void _input_remove_char(struct inputbox *ib, bool backspace) {
    if (backspace) {
        if (ib->pos == 0) return;
        --ib->pos;
    }
    memmove(ib->text+ib->pos, ib->text+ib->pos+1, ib->text_sz-ib->pos);
    ib->text_sz--;
}

static inline bool _input_isdelim(char c) {
    return isspace(c) || !(isalnum(c) || c == '_');
}

static inline void _input_next_word(struct inputbox *ib) {
    if (ib->pos+1 >= ib->text_sz) return;
    if (_input_isdelim(ib->text[ib->pos+1]))
        while (ib->pos < ib->text_sz-1 && _input_isdelim(ib->text[ib->pos+1])) ++ib->pos;
    else while (ib->pos < ib->text_sz-1 && !_input_isdelim(ib->text[ib->pos+1])) ++ib->pos;
}

static inline void _input_prev_word(struct inputbox *ib) {
    if (ib->pos <= 1) return;
    if (_input_isdelim(ib->text[ib->pos-1]))
        while (ib->pos > 0 && _input_isdelim(ib->text[ib->pos-1])) --ib->pos;
    else while (ib->pos > 0 && !_input_isdelim(ib->text[ib->pos-1])) --ib->pos;
}

static inline void _input_remove_next_word(struct inputbox *ib) {
    if (ib->pos >= ib->text_sz) return;
    int p = ib->pos;
    _input_next_word(ib);
    for (; ib->pos > p && ib->pos > 0; --ib->pos) _input_remove_char(ib, FALSE);
    _input_remove_char(ib, FALSE);
}

static inline void _input_remove_prev_word(struct inputbox *ib) {
    if (ib->pos == 0) return;
    int p = ib->pos;
    _input_prev_word(ib);
    for (; ib->pos < p && ib->pos < ib->text_sz; --p) _input_remove_char(ib, FALSE);
    if (ib->pos < ib->text_sz && !_input_isdelim(ib->text[ib->pos])) _input_remove_char(ib, FALSE);
}

void input_update(struct inputbox *ib, int key) {
    const char *kname = keyname(key);
    switch (key) {
    case KEY_LEFT:
        if (ib->pos > 0) --ib->pos;
        break;
    case KEY_RIGHT:
        if (ib->pos < ib->text_sz) ++ib->pos;
        break;
    case KEY_UP: case KEY_HOME:
        ib->pos = 0;
        break;
    case KEY_DOWN: case KEY_END:
        ib->pos = ib->text_sz;
        break;
    case KEY_DC:
        if (ib->pos >= ib->text_sz) break;
        _input_remove_char(ib, FALSE);
        break;
    case KEY_BACKSPACE:
        _input_remove_char(ib, TRUE);
        break;
    case 127: case 8: // ctrl + backspace
        _input_remove_prev_word(ib);
        break;
    default:
        if (isprint(key)) _input_insert_char(ib, key);
        else if (!strcmp(kname, "kDC5"))  _input_remove_next_word(ib);
        else if (!strcmp(kname, "kLFT5")) _input_prev_word(ib);
        else if (!strcmp(kname, "kRIT5")) _input_next_word(ib);
        break;
    }
}

void input_render(struct inputbox *ib, int x, int y, int w, int attr) {
    char text[w];
    const int cap = (ib->text_sz < w)? ib->text_sz : w;
    const int off = (ib->pos >= w-1)? ib->pos-w+1 : 0;
    memset(text, ' ', sizeof(text));
    memcpy(text, ib->text+off, cap);
    mvprintw(y, x, "%.*s", w, text);
    attrset(attr);
    mvprintw(y, x+ib->pos-off, "%c", isprint(ib->text[ib->pos])? ib->text[ib->pos] : ' ');
    attroff(attr);
}

void input_reset(struct inputbox *ib) {
    ib->text_sz = ib->pos = 0;
    memset(ib->text, 0, sizeof(ib->text));
}

void input_set(struct inputbox *ib, char *text, int sz) {
    input_reset(ib);
    ib->text_sz = ib->pos = (INPUTBOX_TEXT_SIZE < sz)? INPUTBOX_TEXT_SIZE : sz;
    memcpy(ib->text, text, ib->text_sz);
}

#endif

#endif
