#ifndef __LED_H
#define __LED_H

#include <stdlib.h>

#define LENGTH(x) (sizeof(x)/sizeof((x)[0]))
#define CTRL(c) ((c) & 0x1f)
#define ALLOC_SIZE 512
#define MIN(a, b) ((a) < (b)? (a) : (b))
#define MAX(a, b) ((a) > (b)? (a) : (b))
#define MIN_TERM_WIDTH 30
#define MIN_TERM_HEIGHT 5

struct line { uint32_t start, end; };

struct cursor {
    int cur, off, line, sel;
    // XXX: maybe cursor should keep track of the text it's editing
    // (so maybe we could have multiple cursors later?)
};

enum { MODE_NONE, MODE_EXIT, MODE_FIND, MODE_GOTO, MODE_OPEN, MODE_REPLACE };

struct action {
    enum { ACTION_INSERT, ACTION_DELETE, ACTION_BACKSPACE } type;
    struct cursor cur;
    int text_sz, text_cap;
    char *text;
};

struct buffer {
    char *name, *text;
    size_t text_sz, text_cap, lines_sz, lines_cap, actions_sz, actions_cap;
    int action, last_change, is_undo, is_readonly, cur_x, cur_y;
    struct cursor cur;
    struct line *lines;
    struct action *actions;
};

void open_file(char *path, bool readonly);
void close_buffer(struct buffer *buf);
void write_file(struct buffer *buf, char *path);
void exit_program(void);
void scroll_up(struct buffer *buf);
void scroll_down(struct buffer *buf);
void move_left(struct buffer *buf);
void move_right(struct buffer *buf);
void move_up(struct buffer *buf);
void move_down(struct buffer *buf);
void move_home(struct buffer *buf);
void move_end(struct buffer *buf);
void page_up(struct buffer *buf);
void page_down(struct buffer *buf);
void move_next_word(struct buffer *buf);
void move_prev_word(struct buffer *buf);
void insert_text(struct buffer *buf, char *text, int sz);
void insert_char(struct buffer *buf, char ch);
void remove_text(struct buffer *buf, bool backspace, int sz);
void remove_char(struct buffer *buf, bool backspace);
void remove_next_word(struct buffer *buf);
void remove_prev_word(struct buffer *buf);
void indent(struct buffer *buf);
void unindent(struct buffer *buf);
void indent_selection(struct buffer *buf);
void unindent_selection(struct buffer *buf);
void undo_action(struct buffer *buf);
void redo_action(struct buffer *buf);
struct line get_selection(struct buffer *buf);
bool is_selecting(struct buffer *buf);
void remove_selection(struct buffer *buf);
void copy_selection(struct buffer *buf);
void paste_text(struct buffer *buf);
void find_string(struct buffer *buf, char *to_find);
void replace_string(struct buffer *buf, char *to_replace, char *str);
void goto_line(struct buffer *buf, long line);

#endif
