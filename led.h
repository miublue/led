#ifndef __LED_H
#define __LED_H

#include <stdlib.h>
#include "inputbox.h"

#define LENGTH(x) (sizeof(x)/sizeof((x)[0]))
#define CTRL(c) ((c) & 0x1f)
#define ALLOC_SIZE 512

#ifndef _USE_COLOR
#define _USE_COLOR 0
#endif
// XXX: make colors (and, perhaps, syntax highlighting) customizable at runtime
#if _USE_COLOR
#define COLOR_STATUS     COLOR_RED
#define COLOR_IDENTIFIER COLOR_WHITE
#define COLOR_KEYWORD    COLOR_YELLOW
#define COLOR_OPERATOR   COLOR_YELLOW
#define COLOR_LITERAL    COLOR_GREEN
#define COLOR_COMMENT    COLOR_CYAN

#define ATTR_STATUS     A_BOLD
#define ATTR_IDENTIFIER 0
#define ATTR_KEYWORD    0
#define ATTR_OPERATOR   0
#define ATTR_LITERAL    0
#define ATTR_COMMENT    A_ITALIC
#else
#define ATTR_STATUS     A_BOLD
#define ATTR_IDENTIFIER 0
#define ATTR_KEYWORD    A_DIM|A_BOLD
#define ATTR_OPERATOR   A_DIM
#define ATTR_LITERAL    A_DIM
#define ATTR_COMMENT    A_DIM|A_ITALIC
#endif

enum { PAIR_NORMAL = 0, PAIR_STATUS, PAIR_LITERAL, PAIR_KEYWORD, PAIR_OPERATOR, PAIR_COMMENT };

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
    enum { ACTION_INSERT, ACTION_DELETE, ACTION_BACKSPACE, ACTION_TOUPPER, ACTION_TOLOWER } type;
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
void insert_text(char *buf, int sz);
void insert_char(char ch);
void remove_char(bool backspace);
void remove_next_word(void);
void remove_prev_word(void);
void indent(void);
void unindent(void);
void indent_selection(void);
void unindent_selection(void);
void undo_action(void);
void redo_action(void);
selection_t get_selection(void);
bool is_selecting(void);
void remove_selection(void);
void copy_selection(void);
void paste_text(void);
void word_to_lower(void);
void word_to_upper(void);
void find_string(char *to_find);
void replace_string(char *to_replace, char *str);
void goto_line(long line);
void parse_tokens(void);
void load_syntax(char *name);

#endif
