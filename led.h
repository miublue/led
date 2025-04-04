#ifndef __LED_H
#define __LED_H

#include <stdlib.h>
#include "inputbox.h"

#define CTRL(c) ((c) & 0x1f)
#define ALLOC_SIZE 512

typedef struct line_t selection_t;

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
