#ifndef __LED_H
#define __LED_H

#include <stdlib.h>

#define VERSION "1.53"
#define LENGTH(x) (sizeof(x)/sizeof((x)[0]))
#define CTRL(c) ((c) & 0x1f)
#define ALLOC_SIZE 512
#define MIN(a, b) ((a) < (b)? (a) : (b))
#define MAX(a, b) ((a) > (b)? (a) : (b))
#define MIN_TERM_WIDTH 30
#define MIN_TERM_HEIGHT 5

#define STATUS_FILENAME 0 /* draw only file name */
#define STATUS_FILEPATH 1 /* draw shortened file path */
#define STATUS_FULLPATH 2 /* draw full file path with tilde expansion */

struct line { int start, end; };
struct cursor { int cur, off, line, sel; };

enum { MODE_NONE, MODE_EXIT, MODE_FIND, MODE_REPLACE, MODE_GOTO, MODE_PICKER, MODE_BUFFERS };
enum { ACTION_INSERT, ACTION_DELETE, ACTION_BACKSPACE };
struct action {
    int type, text_sz, text_cap;
    struct cursor cur;
    char *text;
};

struct buffer {
    char *name, *text;
    int text_sz, text_cap, lines_sz, lines_cap, actions_sz, actions_cap;
    int action, last_change, is_undo, is_readonly, cur_x, cur_y;
    struct cursor cur;
    struct line *lines, search_range;
    struct action *actions;
};

struct buffer *create_buffer(char *name);
void close_buffer(struct buffer *buf);
void open_file(char *path, bool readonly);
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
void indent_line(struct buffer *buf);
void unindent_line(struct buffer *buf);
void indent_selection(struct buffer *buf);
void unindent_selection(struct buffer *buf);
void undo_action(struct buffer *buf);
void redo_action(struct buffer *buf);
struct line get_selection(struct buffer *buf);
bool is_selecting(struct buffer *buf);
void remove_selection(struct buffer *buf);
void copy_selection(struct buffer *buf);
void paste_text(struct buffer *buf);
void switch_mode(struct buffer *buf, int mode);
void find_string(struct buffer *buf, char *to_find);
void replace_string(struct buffer *buf, char *to_replace, char *str);
void goto_line(struct buffer *buf, long line);
char *get_filename(const char *name, int fmt_type);

#endif
