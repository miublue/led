#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ncurses.h>
#include "inputbox.h"
#include "led.h"
#include "config.h"
#include "callbacks.h"

// ABANDON ALL HOPE, YE WHO ENTER HERE

static struct {
    size_t text_sz, text_alloc;
    size_t lines_sz, lines_alloc;
    size_t action, actions_sz, actions_alloc;
    int mode, readonly, ww, wh;
    cursor_t cur;
    bool is_undo;
    char *text, *file;
    line_t *lines;
    action_t *actions;
    inputbox_t input, input_find;
} led;

static void actions_append(action_t act) {
    if (led.action+1 < led.actions_sz) {
        for (int i = led.action+1; i < led.actions_sz; ++i) {
            free(led.actions[i].text);
            led.actions[i].text_sz = 0;
        }
        led.actions_sz = led.action+1;
    }
    if (++led.actions_sz >= led.actions_alloc)
        led.actions = realloc(led.actions, sizeof(action_t) * (led.actions_alloc += ALLOC_SIZE));
    led.actions[led.action = led.actions_sz-1] = act;
}

static void free_actions(void) {
    for (int i = 0; i < led.actions_sz; ++i) {
        if (led.actions[i].text_sz) free(led.actions[i].text);
    }
    free(led.actions);
}

static void insert_to_action(action_t *act, char c) {
    if (act->text_sz >= act->text_alloc)
        act->text = realloc(act->text, sizeof(char) * (act->text_alloc += ALLOC_SIZE));
    act->text[act->text_sz++] = c;
}

static void append_action(int type, char c) {
    if (led.action != -1 && led.actions[led.action].type == type) {
        action_t *act = &led.actions[led.action];
        bool is_ins = (type == ACTION_INSERT    && led.cur.cur == act->cur.cur+act->text_sz);
        bool is_del = (type == ACTION_DELETE    && led.cur.cur == act->cur.cur);
        bool is_bak = (type == ACTION_BACKSPACE && led.cur.cur == act->cur.cur-1);
        if (is_bak) act->cur = led.cur;
        if (is_ins || is_del || is_bak) return insert_to_action(act, c);
    }
    action_t act = { .type = type, .cur = led.cur };
    act.text = malloc(sizeof(char) * (act.text_alloc = ALLOC_SIZE));
    act.text_sz = 0;
    insert_to_action(&act, c);
    actions_append(act);
}

static void undo_insert(action_t *act) {
    for (int i = 0; i < act->text_sz; ++i) remove_char(FALSE);
}

static void undo_delete(action_t *act) {
    for (int i = 0; i < act->text_sz; ++i) insert_char(act->text[i]);
}

static void undo_backspace(action_t *act) {
    for (int i = act->text_sz-1; i >= 0; --i) insert_char(act->text[i]);
}

void undo_action(void) {
    if (led.action == -1) return;
    action_t *act = &led.actions[led.action--];
    led.cur = act->cur;
    led.is_undo = TRUE;
    switch (act->type) {
    case ACTION_INSERT: undo_insert(act); break;
    case ACTION_DELETE: undo_delete(act); break;
    case ACTION_BACKSPACE: undo_backspace(act); break;
    default: break;
    }
    led.is_undo = FALSE;
}

void redo_action(void) {
    if (led.action+1 == led.actions_sz) return;
    action_t *act = &led.actions[++led.action];
    led.cur = act->cur;
    led.is_undo = TRUE;
    switch (act->type) {
    case ACTION_INSERT: undo_delete(act); break;
    case ACTION_DELETE: undo_insert(act); break;
    case ACTION_BACKSPACE: undo_insert(act); break;
    default: break;
    }
    led.is_undo = FALSE;
}

static void append_line(line_t line) {
    if (++led.lines_sz >= led.lines_alloc)
        led.lines = realloc(led.lines, sizeof(line_t) * (led.lines_alloc += ALLOC_SIZE));
    led.lines[led.lines_sz-1] = line;
}

static void count_lines(void) {
    size_t line_start = led.lines_sz = 0;
    for (int i = 0; i < led.text_sz; ++i) {
        if (led.text[i] == '\n') {
            append_line((line_t) { line_start, i });
            line_start = i+1;
        }
    }
    if (!isspace(led.text[led.text_sz-1]) || !led.lines_sz) {
        led.cur.cur = led.text_sz;
        insert_char('\n');
        led.cur.cur = 0;
    }
}

static void init_curses(void) {
    initscr();
    raw();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
}

static void quit_curses(void) {
    endwin();
    curs_set(1);
}

void exit_program(void) {
    if (led.file) free(led.file);
    if (led.text) free(led.text);
    if (led.lines) free(led.lines);
    if (led.actions) free_actions();
    quit_curses();
    exit(0);
}

static void load_config_file() {
    char path[ALLOC_SIZE] = {0};
    sprintf(path, "%s/.ledrc", getenv("HOME"));
    cfg_parse_file(path);
}

void open_file(char *path) {
    if (led.file) free(led.file);
    if (led.text) free(led.text);
    if (led.lines) free(led.lines);
    if (led.actions) free_actions();
    led.text = NULL;
    led.lines = malloc(sizeof(line_t) * (led.lines_alloc = ALLOC_SIZE));
    led.actions = malloc(sizeof(action_t) * (led.actions_alloc = ALLOC_SIZE));
    led.text_sz = led.lines_sz = led.actions_sz = 0;
    led.cur = (cursor_t) {0};
    led.mode = MODE_NONE;
    led.is_undo = led.readonly = FALSE;
    led.action = -1;
    input_reset(&led.input);
    led.file = path;

    FILE *file = fopen(path, "r+");
    if (!file) file = fopen(path, "w+"); // oof
    if (!file) { // lul
        file = fopen(path, "r");
        led.readonly = TRUE;
    }
    if (!file) goto open_file_fail; // LMFAO
    fseek(file, 0, SEEK_END);
    led.text_alloc = 1 + (led.text_sz = ftell(file));
    rewind(file);
    led.text = calloc(1, led.text_alloc);
    if (!led.text) goto open_file_fail;
    if (fread(led.text, 1, led.text_sz, file) != led.text_sz) goto open_file_fail;
    count_lines();
    fclose(file);
    load_config_file();
    return;
open_file_fail:
    if (file) fclose(file);
    fprintf(stderr, "could not open file: %s\n", path);
    exit_program();
}

void write_file(char *path) {
    if (led.readonly) return;
    FILE *file = fopen(path, "w");
    fwrite(led.text, 1, led.text_sz, file);
    fclose(file);
}

bool is_selecting(void) {
    return (led.cur.cur != led.cur.sel);
}

selection_t get_selection(void) {
    return (selection_t) {
        .start = MIN(led.cur.cur, led.cur.sel),
        .end = MAX(led.cur.cur, led.cur.sel),
    };
}

void scroll_up(void) {
    if (led.cur.line-led.cur.off < 0 && led.cur.off > 0) --led.cur.off;
}

void scroll_down(void) {
    if (led.cur.line-led.cur.off > led.wh-2) ++led.cur.off;
}

void move_left(void) {
    if (led.cur.cur == 0) return;
    if (led.text[--led.cur.cur] == '\n' && led.cur.line > 0) --led.cur.line;
    scroll_up();
}

void move_right(void) {
    if (led.cur.cur >= led.text_sz-1) return;
    if (++led.cur.cur == led.lines[led.cur.line].end+1 && led.cur.line < led.lines_sz) ++led.cur.line;
    scroll_down();
}

void move_up(void) {
    if (led.cur.line == 0) return;
    --led.cur.line;
    led.cur.cur -= led.lines[led.cur.line].end - led.lines[led.cur.line].start + 1;
    if (led.cur.cur > led.lines[led.cur.line].end) led.cur.cur = led.lines[led.cur.line].end;
    scroll_up();
}

void move_down(void) {
    if (led.cur.line >= led.lines_sz-1) return;
    led.cur.cur += led.lines[led.cur.line].end - led.lines[led.cur.line].start + 1;
    ++led.cur.line;
    if (led.cur.cur > led.lines[led.cur.line].end) led.cur.cur = led.lines[led.cur.line].end;
    scroll_down();
}

void move_home(void) {
    led.cur.cur = led.lines[led.cur.line].start;
}

void move_end(void) {
    led.cur.cur = led.lines[led.cur.line].end;
}

void page_up(void) {
    for (int i = 0; i < led.wh-2; ++i) move_up();
}

void page_down(void) {
    for (int i = 0; i < led.wh-2; ++i) move_down();
}

static inline bool isdelim(char c) {
    return isspace(c) || !(isalnum(c) || c == '_');
    // return (isspace(c) || ispunct(c)) && c != '_';
}

void move_next_word(void) {
    if (led.cur.cur+1 >= led.text_sz) return;
    if (isdelim(led.text[led.cur.cur+1]))
        while (led.cur.cur < led.text_sz-2 && isdelim(led.text[led.cur.cur+1])) move_right();
    else while (led.cur.cur < led.text_sz-2 && !isdelim(led.text[led.cur.cur+1])) move_right();
}

void move_prev_word(void) {
    if (led.cur.cur <= 1) return;
    if (isdelim(led.text[led.cur.cur-1]))
        while (led.cur.cur > 1 && isdelim(led.text[led.cur.cur-1])) move_left();
    else while (led.cur.cur > 1 && !isdelim(led.text[led.cur.cur-1])) move_left();
}

// XXX: allow for adding/removing multiple characters at a time
void insert_char(int ch) {
    if (led.readonly) return;
    if (++led.text_sz >= led.text_alloc)
        led.text = realloc(led.text, (led.text_alloc += ALLOC_SIZE));
    if (!led.is_undo) append_action(ACTION_INSERT, ch);
    memmove(led.text+led.cur.cur+1, led.text+led.cur.cur, led.text_sz-led.cur.cur);
    led.text[led.cur.cur] = ch;
    count_lines();
    move_right();
}

void insert_tab(void) {
    if (led.readonly) return;
    int cur = led.cur.cur, add = 1;
    led.cur.cur = led.lines[led.cur.line].start;
    if (cfg_get_value_idx(EXPAND_TAB)->as_bool && strcmp(led.file, "Makefile") != 0) {
        for (add = 0; add < cfg_get_value_idx(TAB_WIDTH)->as_int; ++add)
            insert_char(' ');
    } else {
        insert_char('\t');
    }
    led.cur.cur = cur + add;
    if (led.cur.cur > led.lines[led.cur.line].end) led.cur.cur = led.lines[led.cur.line].end;
    if (led.cur.cur < led.lines[led.cur.line].start) led.cur.cur = led.lines[led.cur.line].start;
}

void remove_tab(void) {
    if (led.readonly) return;
    int cur = led.cur.cur, rem = 1;
    led.cur.cur = led.lines[led.cur.line].start;
    if (led.text[led.cur.cur] == '\t') {
        remove_char(FALSE);
    } else {
        for (rem = 0; rem < cfg_get_value_idx(TAB_WIDTH)->as_int && led.text[led.cur.cur] == ' '; ++rem)
            remove_char(FALSE);
    }
    led.cur.cur = cur - rem;
    if (led.cur.cur > led.lines[led.cur.line].end) led.cur.cur = led.lines[led.cur.line].end;
    if (led.cur.cur < led.lines[led.cur.line].start) led.cur.cur = led.lines[led.cur.line].start;
}

void remove_char(bool backspace) {
    if (led.readonly) return;
    if (backspace) move_left();
    if (led.text_sz == 0 || led.cur.cur >= led.text_sz-1) return;
    if (!led.is_undo) append_action(backspace? ACTION_BACKSPACE : ACTION_DELETE, led.text[led.cur.cur]);
    memmove(led.text+led.cur.cur, led.text+led.cur.cur+1, led.text_sz-led.cur.cur);
    --led.text_sz;
    count_lines();
}

void find_string(char *to_find) {
    char *str = NULL;
    if ((str = strstr(led.text+led.cur.cur+1, to_find))) {
        while (led.text+led.cur.cur != str) move_right();
    } else if ((str = strstr(led.text, to_find))) {
        led.cur = (cursor_t) {0};
        while (led.text+led.cur.cur != str) move_right();
    } else return;
    led.cur.sel = led.cur.cur;
    led.cur.cur += strlen(to_find)-1;
}

void replace_string(char *to_replace, char *str) {
    int m = MIN(led.cur.cur, led.cur.sel);
    int rep_sz = strlen(to_replace);
    if (!strncmp(led.text+m, to_replace, rep_sz)) {
        led.cur.cur = m;
        for (int i = 0; i < rep_sz; ++i)
            remove_char(FALSE);
        for (int i = 0; i < strlen(str); ++i)
            insert_char(str[i]);
    }
}

void goto_line(long line) {
    if (line == 0) return;
    led.cur.cur = led.cur.off = led.cur.line = 0;
    for (int i = 0; i < line-1; ++i) move_down();
    led.cur.sel = led.cur.cur;
}

static void goto_start_of_selection(void) {
    selection_t sel = get_selection();
    if (led.cur.cur == sel.start) return;
    while (led.cur.cur > sel.start) move_left();
}

void remove_sel(void) {
    if (led.readonly) return;
    if (!is_selecting()) {
        led.cur.cur = led.lines[led.cur.line].start;
        led.cur.sel = led.lines[led.cur.line].end;
    }
    selection_t sel = get_selection();
    goto_start_of_selection();
    for (int i = sel.start; i <= sel.end; ++i)
        remove_char(FALSE);
}

void copy_sel(void) {
    int cur = led.cur.cur;
    bool not_sel = FALSE;
    if (!is_selecting()) {
        not_sel = TRUE;
        led.cur.cur = led.lines[led.cur.line].start;
        led.cur.sel = led.lines[led.cur.line].end;
    }
    selection_t sel = get_selection();
    led.cur.cur = cur;
    if (not_sel) led.cur.sel = led.cur.cur;
    char path[1024] = {0};
    char cmd[2048] = {0};
    sprintf(path, "%s/.ledsel", getenv("HOME"));
    FILE *file = fopen(path, "w");
    fwrite(led.text+sel.start, 1, sel.end-sel.start+1, file);
    fclose(file);
    sprintf(cmd, "cat '%s' | xsel -b", path);
    if (system(cmd));
}

static inline bool is_selected(int i) {
    selection_t sel = get_selection();
    return (is_selecting() && i >= sel.start && i <= sel.end);
}

static void render_line(int l, int off) {
    line_t line = led.lines[l];
    int sz = off;
    const int tab_width = cfg_get_value_idx(TAB_WIDTH)->as_int;
    for (int i = line.start; i <= line.end; ++i) {
        if (led.cur.cur == i || is_selected(i)) attron(A_REVERSE);
        mvprintw(l-led.cur.off, sz, "%c", isspace(led.text[i])? ' ':led.text[i]);
        if (led.text[i] == '\t') {
            if (is_selected(i)) {
                for (int j = 0; j < tab_width; ++j)
                    mvprintw(l-led.cur.off, sz+j, " ");
            }
            sz += tab_width;
        } else sz++;
        attroff(A_REVERSE);
    }
    if (cfg_get_value_idx(LINE_NUMBER)->as_bool)
        mvprintw(l-led.cur.off, 0, " %d ", l+1);
}

static inline int calculate_line_size(void) {
    int sz = 0;
    for (int i = led.lines[led.cur.line].start; i < led.cur.cur; ++i)
        sz += (led.text[i] == '\t')? cfg_get_value_idx(TAB_WIDTH)->as_int : 1;
    return sz;
}

static void render_text(void) {
    erase();
    int cur_off = calculate_line_size(), off = 0;
    while (led.cur.line-led.cur.off < 0) { move_down(); led.cur.sel = led.cur.cur; }
    while (led.cur.line-led.cur.off >= led.wh-1) { move_up(); led.cur.sel = led.cur.cur; }
    if (cfg_get_value_idx(LINE_NUMBER)->as_bool) {
        char linenu[16];
        sprintf(linenu, " %d ", led.lines_sz);
        off = strlen(linenu);
    }
    if (cur_off+off > led.ww-2) off = (led.ww-2)-cur_off;
    for (int i = led.cur.off; i < led.cur.off+led.wh-1; ++i) {
        if (i >= led.lines_sz) break;
        render_line(i, off);
    }
}

static inline char *mode_to_cstr(void) {
    switch (led.mode) {
    case MODE_FIND: return "Find: ";
    case MODE_GOTO: return "Goto: ";
    case MODE_OPEN: return "Open: ";
    case MODE_COMMAND: return "Command: ";
    case MODE_REPLACE: {
        static char str[ALLOC_SIZE] = {0};
        sprintf(str, "Replace(%.*s): ", led.input_find.text_sz, led.input_find.text);
        return str;
    }
    default: return "None";
    }
}

static void render_status(void) {
    char status[ALLOC_SIZE] = {0};
    sprintf(status, " %s %d %d:%ld %s ",
        led.readonly? "[RO]" : "",
        led.cur.cur-led.lines[led.cur.line].start+1,
        led.cur.line+1, led.lines_sz, led.file);
    const size_t status_sz = strlen(status);
    mvprintw(led.wh-1, led.ww-status_sz, "%s", status);
    if (led.mode != MODE_NONE) {
        char *astr = mode_to_cstr();
        mvprintw(led.wh-1, 0, "%s", astr);
        input_render(&led.input, strlen(astr), led.wh-1, led.ww-status_sz-strlen(astr));
    }
}

static void find_next(inputbox_t input) {
    char *text = strndup(input.text, input.text_sz);
    find_string(text);
    free(text);
}

static void replace_current(void) {
    char *to_replace = strndup(led.input_find.text, led.input_find.text_sz);
    char *replace_with = strndup(led.input.text, led.input.text_sz);
    replace_string(to_replace, replace_with);
    free(to_replace);
    free(replace_with);
}

static void jump_to_line(void) {
    char text[INPUTBOX_TEXT_SIZE] = {0};
    memcpy(text, led.input.text, led.input.text_sz);
    long line = strtol(text, NULL, 0);
    goto_line(line);
}

static bool update_none(int ch) {
    if (ch == CTRL('c') || ch == CTRL('q')) {
        led.mode = MODE_NONE;
        return TRUE;
    }
    return FALSE;
}

static void update_replace(int ch) {
    if (update_none(ch)) {
        led.input = led.input_find;
        return;
    }
    if (ch == '\n' || ch == CTRL('r')) {
        if (led.input_find.text_sz) {
            replace_current();
            find_next(led.input_find);
        }
        return;
    } else if (ch == CTRL('f')) {
        led.input = led.input_find;
        led.mode = MODE_FIND;
        return;
    } else if (ch == CTRL('n')) {
        if (led.input.text_sz && led.input_find.text_sz)
            find_next(led.input_find);
    }
    input_update(&led.input, ch);
}

static void update_find(int ch) {
    if (update_none(ch)) return;
    if (ch == '\n' || ch == CTRL('f') || ch == CTRL('n')) {
        if (led.input.text_sz) find_next(led.input);
        return;
    } else if (ch == CTRL('r') && led.input.text_sz) {
        led.mode = MODE_REPLACE;
        led.input_find = led.input;
        input_reset(&led.input);
        return;
    }
    input_update(&led.input, ch);
}


#define FUPDATE(NAME, BODY) \
static void NAME(int ch) { \
    if (update_none(ch)) return; \
    if (ch == '\n') { \
        if (led.input.text_sz) { BODY; } \
        led.mode = MODE_NONE; return; \
    } \
    input_update(&led.input, ch); \
}

FUPDATE(update_goto, jump_to_line())
FUPDATE(update_open, open_file(strndup(led.input.text, led.input.text_sz)))
FUPDATE(update_command, cfg_parse(led.input.text, led.input.text_sz))

#undef FUPDATE

static void update_insert(int ch) {
    switch (ch) {
    case CTRL('q'):
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
   case CTRL('e'):
        led.mode = MODE_COMMAND;
        input_reset(&led.input);
        break;
    case CTRL('g'):
        led.mode = MODE_GOTO;
        input_reset(&led.input);
        break;
    case CTRL('o'):
        led.mode = MODE_OPEN;
        input_reset(&led.input);
        break;
    case CTRL('r'):
    case CTRL('f'):
        led.mode = MODE_FIND;
        input_reset(&led.input);
        break;
    case CTRL('n'):
        if (led.input.text_sz) find_next(led.input);
        return;
    case CTRL('z'):
        undo_action(); break;
    case CTRL('y'):
        redo_action(); break;
    case KEY_LEFT:
        move_left(); break;
    case KEY_SLEFT:
        return move_left();
    case KEY_RIGHT:
        move_right(); break;
    case KEY_SRIGHT:
        return move_right();
    // XXX: these keys differ depending on the keyboard/terminal
    case 560: case 569: case 572: // ctrl + right
        move_next_word(); break;
    case 561: case 570: case 573: // ctrl + shift + right
        return move_next_word();
    case 545: case 554: case 557: // ctrl + left
        move_prev_word(); break;
    case 546: case 555: case 558: // ctrl + shift + left
        return move_prev_word();
    case KEY_UP:
        move_up(); break;
    case KEY_SR:
        return move_up();
    case KEY_DOWN:
        move_down(); break;
    case KEY_SF:
        return move_down();
    case KEY_HOME:
        move_home(); break;
    case KEY_SHOME:
        return move_home();
    case KEY_END:
        move_end(); break;
    case KEY_SEND:
        return move_end();
    case KEY_PPAGE:
        page_up(); break;
    case KEY_SPREVIOUS:
        return page_up();
    case KEY_NPAGE:
        page_down(); break;
    case KEY_SNEXT:
        return page_down();
    case '\n':
        if (is_selecting()) remove_sel();
        insert_char('\n'); break;
    case '\t':
        insert_tab(); break;
    case KEY_BTAB:
        remove_tab(); break;
    case KEY_DC:
        if (is_selecting()) remove_sel();
        else remove_char(FALSE);
        break;
    case KEY_BACKSPACE:
        if (is_selecting()) remove_sel();
        else {
            if (led.cur.cur == 0) break;
            remove_char(TRUE);
        }
        break;
    case 528: case 531: // ctrl + del
        if (is_selecting()) remove_sel();
        else {
            move_next_word();
            remove_sel();
        }
        break;
    case 8: // ctrl + backspace
        if (is_selecting()) remove_sel();
        else {
            if (led.cur.cur == 0) break;
            if (isdelim(led.text[led.cur.cur])) move_left();
            led.cur.sel = led.cur.cur;
            move_prev_word();
            remove_sel();
        }
        break;
    default:
        if (!isprint(ch)) break;
        if (is_selecting()) remove_sel();
        insert_char(ch);
        break;
    }
    led.cur.sel = led.cur.cur;
}

static void update(void) {
    void (*update_fns[])(int) = {
        [MODE_NONE]    = &update_insert,
        [MODE_FIND]    = &update_find,
        [MODE_GOTO]    = &update_goto,
        [MODE_OPEN]    = &update_open,
        [MODE_REPLACE] = &update_replace,
        [MODE_COMMAND] = &update_command,
    };
    int ch = getch();
    return update_fns[led.mode](ch);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <file>\n", argv[0]);
        return 1;
    }
    open_file(strdup(argv[1]));
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
