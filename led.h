#ifndef __LED_H
#define __LED_H

#include <stdlib.h>
#include "inputbox.h"

#define CTRL(c) ((c) & 0x1f)
#define ALLOC_SIZE 512

typedef struct line_t selection_t;
typedef struct line_t {
    uint32_t start, end;
} line_t;

typedef struct cursor_t {
    int cur, off, line, sel;
    // XXX: maybe cursor should keep track of the text it's editing
    // (so maybe we could have multiple cursors later?)
} cursor_t;

enum { MODE_NONE, MODE_FIND, MODE_GOTO, MODE_OPEN, MODE_REPLACE, MODE_COMMAND };

typedef struct {
    enum { ACTION_INSERT, ACTION_DELETE, ACTION_BACKSPACE } type;
    cursor_t cur;
    int text_sz, text_alloc;
    char *text;
} action_t;

void open_file(char *path);
void write_file(char *path);
void exit_program(void);
void scroll_up(void);
void scroll_down(void);
void move_left(void);
void move_right(void);
void move_up(void);
void move_down(void);
void move_home(void);
void move_end(void);
void page_up(void);
void page_down(void);
void move_next_word(void);
void move_prev_word(void);
void insert_char(int ch);
void insert_tab(void);
void remove_tab(void);
void remove_char(bool backspace);
void undo_action(void);
void redo_action(void);
selection_t get_selection(void);
bool is_selecting(void);
void remove_sel(void);
void copy_sel(void);
void find_string(char *to_find);
void replace_string(char *to_replace, char *str);
void goto_line(long line);

#endif
