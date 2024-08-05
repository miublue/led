#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ncurses.h>
#include "inputbox.h"

#define CTRL(c) ((c) & 0x1f)
#define ALLOC_SIZE 512
#define TAB_WIDTH 4
#define TABS_TO_SPACES TRUE

typedef struct {
    uint32_t start, end;
} line_t;

typedef line_t selection_t;

enum {
    ACTION_NONE,
    ACTION_FIND,
    // TODO: ACTION_REPLACE, ACTION_OPEN
};

typedef uint8_t action_t;

struct {
    size_t text_sz, text_alloc;
    size_t lines_sz, lines_alloc;
    int cur, sel, off, line, ww, wh;
    char *text, *file;
    line_t *lines;
    action_t action;
    inputbox_t input;
} led;

void append_line(line_t line);
void count_lines();

void init_curses();
void quit_curses();
void exit_program();
void open_file(char *path);
void write_file(char *path);

void scroll_up();
void scroll_down();
void move_left();
void move_right();
void move_up();
void move_down();
void page_up();
void page_down();
void move_next_word();
void move_prev_word();
void insert_char(int ch);
void insert_tab();
void remove_char();
void remove_sel();
void copy_sel();

void update();
void render_text();
void render_status();

void append_line(line_t line) {
    if (++led.lines_sz >= led.lines_alloc)
        led.lines = realloc(led.lines, sizeof(line_t) * (led.lines_alloc += ALLOC_SIZE));
    led.lines[led.lines_sz-1] = line;
}

void count_lines() {
    size_t line_start = led.lines_sz = 0;
    for (int i = 0; i < led.text_sz; ++i) {
        if (led.text[i] == '\n') {
            append_line((line_t) { line_start, i });
            line_start = i+1;
        }
    }
    if (!led.lines_sz) {
        led.cur = led.text_sz;
        insert_char('\n');
        led.cur = 0;
    }
}

void init_curses() {
    initscr();
    raw();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
}

void quit_curses() {
    endwin();
    curs_set(1);
}

void exit_program() {
    if (led.text) free(led.text);
    if (led.lines) free(led.lines);
    exit(0);
}

void open_file(char *path) {
    if (led.text) free(led.text);
    if (led.lines) free(led.lines);
    led.text = NULL;
    led.lines = malloc(sizeof(line_t) * (led.lines_alloc = ALLOC_SIZE));
    led.cur = led.sel = led.off = led.line = led.text_sz = led.lines_sz = 0;
    led.action = ACTION_NONE;
    input_reset(&led.input);
    led.file = path;

    FILE *file = fopen(path, "r+");
    if (!file) {
        file = fopen(path, "w+");
        if (!file) goto open_file_fail;
    }
    fseek(file, 0, SEEK_END);
    led.text_alloc = 1 + (led.text_sz = ftell(file));
    rewind(file);
    led.text = calloc(1, led.text_alloc);
    if (!led.text) goto open_file_fail;
    if (fread(led.text, 1, led.text_sz, file) != led.text_sz) goto open_file_fail;
    count_lines();
    fclose(file);
    return;

open_file_fail:
    if (file) fclose(file);
    fprintf(stderr, "could not open file: %s\n", path);
    exit_program();
}

void write_file(char *path) {
    FILE *file = fopen(path, "w");
    fwrite(led.text, 1, led.text_sz, file);
    fclose(file);
}

static inline bool is_selecting() {
    return (led.cur != led.sel);
}

static inline selection_t get_selection() {
    return (selection_t) {
        .start = MIN(led.cur, led.sel),
        .end = MAX(led.cur, led.sel),
    };
}

void scroll_up() {
    if (led.line-led.off < 0 && led.off > 0)
        --led.off;
}

void scroll_down() {
    if (led.line-led.off > led.wh-2)
        ++led.off;
}

void move_left() {
    if (led.cur == 0) return;
    if (led.text[--led.cur] == '\n')
        if (led.line > 0) --led.line;
    scroll_up();
}

void move_right() {
    if (led.cur >= led.text_sz-1) return;
    if (++led.cur == led.lines[led.line].end+1)
        if (led.line < led.lines_sz) ++led.line;
    scroll_down();
}

void move_up() {
    if (led.line == 0) return;
    --led.line;
    led.cur -= led.lines[led.line].end - led.lines[led.line].start + 1;
    if (led.cur > led.lines[led.line].end) led.cur = led.lines[led.line].end;
    scroll_up();
}

void move_down() {
    if (led.line >= led.lines_sz-1) return;
    led.cur += led.lines[led.line].end - led.lines[led.line].start + 1;
    ++led.line;
    if (led.cur > led.lines[led.line].end) led.cur = led.lines[led.line].end;
    scroll_down();
}

void page_up() {
    for (int i = 0; i < led.wh-2; ++i)
        move_up();
}

void page_down() {
    for (int i = 0; i < led.wh-2; ++i)
        move_down();
}

void move_next_word() {
    if (led.cur >= led.text_sz) return;
    if (isalnum(led.text[led.cur]))
        while (led.cur < led.text_sz-1 && isalnum(led.text[led.cur])) move_right();
    else
        while (led.cur < led.text_sz-1 && !isalnum(led.text[led.cur])) move_right();
}

void move_prev_word() {
    if (led.cur == 0) return;
    if (isalnum(led.text[led.cur]))
        while (led.cur > 0 && isalnum(led.text[led.cur])) move_left();
    else
        while (led.cur > 0 && !isalnum(led.text[led.cur])) move_left();
}

void insert_char(int ch) {
    if (++led.text_sz >= led.text_alloc)
        led.text = realloc(led.text, (led.text_alloc += ALLOC_SIZE));
    memmove(led.text+led.cur+1, led.text+led.cur, led.text_sz-led.cur);
    led.text[led.cur] = ch;
    count_lines();
    move_right();
}

void insert_tab() {
    if (TABS_TO_SPACES) {
        for (int i = 0; i < TAB_WIDTH; ++i)
            insert_char(' ');
    }
    else {
        insert_char('\t');
    }
}

void remove_char() {
    if (led.text_sz == 0 || led.cur >= led.text_sz-1) return;
    memmove(led.text+led.cur, led.text+led.cur+1, led.text_sz-led.cur);
    --led.text_sz;
    count_lines();
}

static void goto_start_of_selection() {
    selection_t sel = get_selection();
    if (led.cur == sel.start) return;
    while (led.cur > sel.start) move_left();
}

void remove_sel() {
    if (!is_selecting()) {
        led.cur = led.lines[led.line].start;
        led.sel = led.lines[led.line].end;
    }
    selection_t sel = get_selection();
    goto_start_of_selection();
    for (int i = sel.start; i <= sel.end; ++i)
        remove_char();
}

void copy_sel() {
    int cur = led.cur;
    bool not_sel = false;
    if (!is_selecting()) {
        not_sel = true;
        led.cur = led.lines[led.line].start;
        led.sel = led.lines[led.line].end;
    }
    selection_t sel = get_selection();
    led.cur = cur;
    if (not_sel) led.sel = led.cur;
    char path[1024] = {0};
    char cmd[2048] = {0};
    sprintf(path, "%s/.ledsel", getenv("HOME"));
    FILE *file = fopen(path, "w");
    fwrite(led.text+sel.start, 1, sel.end-sel.start+1, file);
    fclose(file);
    sprintf(cmd, "cat '%s' | xsel -b", path);
    system(cmd);
}

static inline bool is_selected(int i) {
    selection_t sel = get_selection();
    return (is_selecting() && i >= sel.start && i <= sel.end);
}

static void render_line(int l, int off) {
    line_t line = led.lines[l];
    int sz = off;
    for (int i = line.start; i <= line.end; ++i) {
        if (led.cur == i || is_selected(i)) attron(A_REVERSE);
        mvprintw(l-led.off, sz, "%c", isprint(led.text[i])? led.text[i] : ' ');
        if (led.text[i] == '\t') {
            if (is_selected(i)) {
                for (int j = 0; j < TAB_WIDTH; ++j)
                    mvprintw(l-led.off, sz+j, " ");
            }
            sz += TAB_WIDTH;
        }
        else sz++;
        attroff(A_REVERSE);
    }
}

void render_text() {
    erase();
    // TODO: make horizontal offset work with tabs
    int off = (led.cur-led.lines[led.line].start > led.ww-2)?
                (led.ww-2)-(led.cur-led.lines[led.line].start) : 0;

    for (int i = led.off; i < led.off+led.wh-1; ++i) {
        if (i >= led.lines_sz) break;
        render_line(i, off);
    }
}

static inline char *action_to_cstr() {
    switch (led.action) {
    case ACTION_FIND: return "Find: ";
    default: return "None";
    }
}

void render_status() {
    char status[ALLOC_SIZE] = {0};
    sprintf(status, " %d %d:%ld %s ",
        led.cur-led.lines[led.line].start+1,
        led.line+1, led.lines_sz, led.file);
    const size_t status_sz = strlen(status);
    mvprintw(led.wh-1, led.ww-status_sz, status);
    if (led.action != ACTION_NONE) {
        char *astr = action_to_cstr();
        mvprintw(led.wh-1, 0, "%s", astr);
        input_render(&led.input, strlen(astr), led.wh-1, led.ww-status_sz-strlen(astr));
    }
}

// keys you press when selecting text
static inline bool selection_keys(int c) {
    return (c == KEY_SLEFT || c == KEY_SRIGHT || c == KEY_SR || c == KEY_SF
        || c == KEY_SHOME || c == KEY_SEND || c == KEY_SPREVIOUS || c == KEY_SNEXT);
}

static void find_next() {
    char text[INPUTBOX_TEXT_SIZE] = {0};
    memcpy(text, led.input.text, led.input.text_sz);
    char *str;
    if (str = strstr(led.text+led.cur+led.input.text_sz, text)) {
        while (led.text+led.cur != str) move_right();
    }
    else if (str = strstr(led.text, text)) {
        led.cur = led.off = led.sel = led.line = 0;
        while (led.text+led.cur != str) move_right();
    }
    else return;
    led.sel = led.cur;
    led.cur += led.input.text_sz-1;
}

static void update_find(int ch) {
    if (ch == CTRL('c') || ch == CTRL('q')) {
        led.action = ACTION_NONE;
        return;
    }
    if (ch == '\n' || ch == CTRL('f') || ch == CTRL('n')) {
        if (led.input.text_sz) find_next();
        return;
    }
    input_update(&led.input, ch);
}

void update() {
    int ch = getch();
    switch (led.action) {
    case ACTION_FIND:
        return update_find(ch);
    default: break;
    }

    switch (ch) {
    case 27: { // alt or esc
        ch = getch(); // alt+key sends another key right after 27
        if (ch == 'f') move_next_word();
        if (ch == 'F') return move_next_word();
        if (ch == 'b') move_prev_word();
        if (ch == 'B') return move_prev_word();
    } break;
    case CTRL('q'):
        quit_curses();
        exit_program();
        break;
    case CTRL('s'):
        write_file(led.file);
        break;
    case CTRL('c'):
        copy_sel();
        break;
    case CTRL('x'):
        copy_sel();
        remove_sel();
        break;
    case CTRL('f'):
        led.action = ACTION_FIND;
        input_reset(&led.input);
        break;
    case CTRL('n'):
        if (led.input.text_sz)
            find_next();
        return;
    case KEY_SLEFT:
    case KEY_LEFT:
        move_left();
        break;
    case KEY_SRIGHT:
    case KEY_RIGHT:
        move_right();
        break;
    case KEY_SR:
    case KEY_UP:
        move_up();
        break;
    case KEY_SF:
    case KEY_DOWN:
        move_down();
        break;
    case KEY_SHOME:
    case KEY_HOME:
        led.cur = led.lines[led.line].start;
        break;
    case KEY_SEND:
    case KEY_END:
        led.cur = led.lines[led.line].end;
        break;
    case KEY_SPREVIOUS:
    case KEY_PPAGE:
        page_up();
        break;
    case KEY_SNEXT:
    case KEY_NPAGE:
        page_down();
        break;
    case '\n':
        insert_char('\n');
        break;
    case '\t':
        insert_tab();
        break;
    case KEY_DC:
        if (is_selecting()) remove_sel();
        else remove_char();
        break;
    case KEY_BACKSPACE:
        if (is_selecting()) remove_sel();
        else {
            if (led.cur == 0) break;
            move_left();
            remove_char();
        }
        break;
    default:
        if (!isprint(ch)) break;
        if (is_selecting()) remove_sel();
        insert_char(ch);
        break;
    }
    if (!selection_keys(ch)) led.sel = led.cur;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <file>\n", argv[0]);
        return 1;
    }
    open_file(argv[1]);
    init_curses();
    for (;;) {
        getmaxyx(stdscr, led.wh, led.ww);
        render_text();
        render_status();
        update();
    }
    quit_curses();
    return 0;
}
