#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ncurses.h>
#include "inputbox.h"
#include "led.h"
#include "config.h"

// ABANDON ALL HOPE, YE WHO ENTER HERE

static struct {
    size_t text_sz, text_alloc;
    size_t lines_sz, lines_alloc;
    size_t actions_sz, actions_alloc;
    int mode, action, last_change, ww, wh;
    int cur_x, cur_y;
    int is_undo, is_readonly;
    cursor_t cur;
    char *text, *file;
    line_t *lines;
    action_t *actions;
    inputbox_t input, input_find;
} led;

static void _actions_append(action_t act) {
    if (led.action+1 < led.actions_sz) {
        for (int i = led.action+1; i < led.actions_sz; ++i) free(led.actions[i].text);
        led.actions_sz = led.action+1;
    }
    if (++led.actions_sz >= led.actions_alloc)
        led.actions = realloc(led.actions, sizeof(action_t) * (led.actions_alloc += ALLOC_SIZE));
    led.actions[led.action = led.actions_sz-1] = act;
}

static void _free_actions(void) {
    for (int i = 0; i < led.actions_sz; ++i)
        if (led.actions[i].text_alloc) free(led.actions[i].text);
    if (led.actions) free(led.actions);
}

static void _insert_to_action(action_t *act, char *buf, int sz) {
    if (act->text_sz+sz >= act->text_alloc)
        act->text = realloc(act->text, sizeof(char) * (act->text_alloc += sz+ALLOC_SIZE));
    memmove(act->text+act->text_sz, buf, sz);
    act->text_sz += sz;
}

static bool _is_action_repeat(int type, action_t *act) {
    switch (type) {
    case ACTION_DELETE: return led.cur.cur == act->cur.cur;
    case ACTION_BACKSPACE: return led.cur.cur == act->cur.cur-1;
    default: return led.cur.cur == act->cur.cur + act->text_sz;
    }
}

static void _append_action(int type, char *buf, int sz) {
    if (led.action != -1 && led.actions[led.action].type == type) {
        action_t *act = &led.actions[led.action];
        bool repeat = _is_action_repeat(type, act);
        if (repeat && type == ACTION_BACKSPACE) act->cur = led.cur;
        if (repeat) return _insert_to_action(act, buf, sz);
    }
    action_t act = { .type = type, .cur = led.cur };
    act.text = malloc(sizeof(char) * (act.text_alloc = sz+ALLOC_SIZE));
    act.text_sz = 0;
    _insert_to_action(&act, buf, sz);
    _actions_append(act);
}

static inline void _undo_insert(action_t *act) {
    led.cur.sel = (led.cur.cur+act->text_sz)-1;
    if (led.cur.sel == led.cur.cur) remove_char(FALSE);
    else remove_selection();
}

static inline void _undo_delete(action_t *act) {
    insert_text(act->text, act->text_sz);
}

static inline void _undo_backspace(action_t *act) {
    char text[act->text_sz+1];
    for (int i = 0; i < act->text_sz; ++i)
        text[(act->text_sz-1)-i] = act->text[i];
    insert_text(text, act->text_sz);
}

void undo_action(void) {
    if (led.action == -1 || led.is_readonly) return;
    action_t *act = &led.actions[led.action--];
    led.cur = act->cur;
    led.is_undo = TRUE;
    switch (act->type) {
    case ACTION_INSERT: _undo_insert(act); break;
    case ACTION_DELETE: _undo_delete(act); break;
    case ACTION_BACKSPACE: _undo_backspace(act); break;
    default: break;
    }
    led.is_undo = FALSE;
}

void redo_action(void) {
    if (led.action+1 == led.actions_sz || led.is_readonly) return;
    action_t *act = &led.actions[++led.action];
    led.cur = act->cur;
    led.is_undo = TRUE;
    switch (act->type) {
    case ACTION_INSERT: _undo_delete(act); break;
    case ACTION_DELETE: _undo_insert(act); break;
    case ACTION_BACKSPACE: _undo_insert(act); break;
    default: break;
    }
    led.is_undo = FALSE;
}

static void _append_line(line_t line) {
    if (++led.lines_sz >= led.lines_alloc)
        led.lines = realloc(led.lines, sizeof(line_t) * (led.lines_alloc += ALLOC_SIZE));
    led.lines[led.lines_sz-1] = line;
}

static void _count_lines(void) {
    size_t line_start = led.lines_sz = 0;
    for (int i = 0; i < led.text_sz; ++i) {
        if (led.text[i] == '\n') {
            _append_line((line_t) { line_start, i });
            line_start = i+1;
        }
    }
    if (!isspace(led.text[led.text_sz-1]) || !led.lines_sz) {
        led.cur.cur = led.text_sz;
        insert_char('\n');
        led.cur.cur = 0;
    }
}

static void _init_curses(void) {
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    getmaxyx(stdscr, led.wh, led.ww);
}

static void _quit_curses(void) {
    endwin();
    curs_set(1);
}

void exit_program(void) {
    if (led.file) free(led.file);
    if (led.text) free(led.text);
    if (led.lines) free(led.lines);
    if (led.actions) _free_actions();
    _quit_curses();
    exit(0);
}

void open_file(char *path, bool is_readonly) {
    struct stat stbuf;
    FILE *file = NULL;
    if (led.file) free(led.file);
    if (led.text) free(led.text);
    if (led.lines) free(led.lines);
    if (led.actions) _free_actions();
    led.text = NULL;
    led.lines = malloc(sizeof(line_t) * (led.lines_alloc = ALLOC_SIZE));
    led.actions = malloc(sizeof(action_t) * (led.actions_alloc = ALLOC_SIZE));
    led.text_sz = led.lines_sz = led.actions_sz = 0;
    led.cur = (cursor_t) {0};
    led.mode = MODE_NONE;
    led.is_undo = led.is_readonly = FALSE;
    led.action = led.last_change = -1;
    input_reset(&led.input);
    led.file = path;
    if (stat(path, &stbuf) != 0) {
        // try creating file if it cannot stat it, exit if that also fails
        if (!(file = fopen(path, "w+"))) goto open_file_fail;
        if (stat(path, &stbuf) != 0) goto open_file_fail;
    }
    if (is_readonly || !(stbuf.st_mode & S_IWUSR)) led.is_readonly = TRUE;
    if (!file) file = fopen(path, "r");
    fseek(file, 0, SEEK_END);
    led.text_alloc = 1 + (led.text_sz = ftell(file));
    rewind(file);
    led.text = calloc(1, led.text_alloc);
    if (!led.text) goto open_file_fail;
    if (fread(led.text, 1, led.text_sz, file) != led.text_sz) goto open_file_fail;
    _count_lines();
    fclose(file);
    return;
open_file_fail:
    if (file) fclose(file);
    fprintf(stderr, "error: could not open file '%s'\n", path);
    exit_program();
}

void write_file(char *path) {
    if (led.is_readonly) return;
    led.last_change = led.action;
    if (!path) path = led.file;
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

static inline bool _is_delim(char c) {
    return isspace(c) || !(isalnum(c) || c == '_');
}

void move_next_word(void) {
    if (led.cur.cur+1 >= led.text_sz) return;
    if (_is_delim(led.text[led.cur.cur+1]))
        while (led.cur.cur < led.text_sz-2 && _is_delim(led.text[led.cur.cur+1])) move_right();
    else while (led.cur.cur < led.text_sz-2 && !_is_delim(led.text[led.cur.cur+1])) move_right();
}

void move_prev_word(void) {
    if (led.cur.cur <= 1) return;
    if (_is_delim(led.text[led.cur.cur-1]))
        while (led.cur.cur > 1 && _is_delim(led.text[led.cur.cur-1])) move_left();
    else while (led.cur.cur > 1 && !_is_delim(led.text[led.cur.cur-1])) move_left();
}

// XXX: UTF-8 lmao
void insert_text(char *buf, int sz) {
    if (led.is_readonly) return;
    if (led.text_sz+sz >= led.text_alloc)
        led.text = realloc(led.text, sizeof(char) * (led.text_alloc += sz+ALLOC_SIZE));
    memmove(led.text+led.cur.cur+sz, led.text+led.cur.cur, led.text_sz-led.cur.cur);
    memmove(led.text+led.cur.cur, buf, sz);
    led.text_sz += sz;
    _count_lines();
    if (!led.is_undo) _append_action(ACTION_INSERT, buf, sz);
    for (int i = 0; i < sz; ++i) move_right();
}

void insert_char(char ch) {
    insert_text(&ch, 1);
}

void remove_text(bool backspace, int sz) {
    if (led.is_readonly) return;
    if (backspace) for (int i = 0; i < sz; ++i) move_left();
    if (led.text_sz == 0 || led.cur.cur >= led.text_sz-1) return;
    if (!led.is_undo) _append_action(backspace? ACTION_BACKSPACE : ACTION_DELETE, led.text+led.cur.cur, sz);
    memmove(led.text+led.cur.cur, led.text+led.cur.cur+sz, led.text_sz-led.cur.cur);
    led.text_sz -= sz;
    _count_lines();
}

void remove_char(bool backspace) {
    remove_text(backspace, 1);
}

void remove_prev_word(void) {
    if (led.cur.cur == 0) return;
    if (_is_delim(led.text[led.cur.cur])) move_left();
    led.cur.sel = led.cur.cur;
    move_prev_word();
    remove_selection();
}

void remove_next_word(void) {
    move_next_word();
    remove_selection();
}

void indent(void) {
    if (led.is_readonly) return;
    int cur = led.cur.cur, add = 1;
    led.cur.cur = led.lines[led.cur.line].start;
    // XXX: some filetype detection would be pretty handy later on
    if (!CFG_EXPANDTAB || !strcasecmp(led.file, "makefile"))
        insert_char('\t');
    else for (add = 0; add < CFG_TABWIDTH; ++add)
        insert_char(' ');
    led.cur.cur = cur + add;
    if (led.cur.cur > led.lines[led.cur.line].end) led.cur.cur = led.lines[led.cur.line].end;
    if (led.cur.cur < led.lines[led.cur.line].start) led.cur.cur = led.lines[led.cur.line].start;
}

void unindent(void) {
    if (led.is_readonly) return;
    int cur = led.cur.cur, rem = 1;
    led.cur.cur = led.lines[led.cur.line].start;
    if (led.text[led.cur.cur] == '\t')
        remove_char(FALSE);
    else for (rem = 0; rem < CFG_TABWIDTH && led.text[led.cur.cur] == ' '; ++rem)
        remove_char(FALSE);
    led.cur.cur = cur - rem;
    if (led.cur.cur > led.lines[led.cur.line].end) led.cur.cur = led.lines[led.cur.line].end;
    if (led.cur.cur < led.lines[led.cur.line].start) led.cur.cur = led.lines[led.cur.line].start;
}

static char *_casestrstr(const char *haystack, const char *needle) {
    if (CFG_IGNORECASE) return strcasestr(haystack, needle);
    return strstr(haystack, needle);
}

// XXX: move line to the center of the screen on find
void find_string(char *to_find) {
    char *str = NULL;
    if ((str = _casestrstr(led.text+led.cur.cur+1, to_find))) {
        while (led.text+led.cur.cur != str && led.cur.cur < led.text_sz) move_right();
    } else if ((str = _casestrstr(led.text, to_find))) {
        led.cur = (cursor_t) {0};
        while (led.text+led.cur.cur != str && led.cur.cur < led.text_sz) move_right();
    } else return;
    led.cur.sel = led.cur.cur;
    led.cur.cur += strlen(to_find)-1;
}

void replace_string(char *to_replace, char *str) {
    int m = MIN(led.cur.cur, led.cur.sel);
    int rep_sz = strlen(to_replace), str_sz = strlen(str);
    if (!strncmp(led.text+m, to_replace, rep_sz)) {
        led.cur.cur = m;
        remove_text(FALSE, rep_sz);
        if (str_sz) insert_text(str, str_sz);
    }
}

void goto_line(long line) {
    if (line == 0) return;
    led.cur.cur = led.cur.off = led.cur.line = 0;
    for (int i = 0; i < MIN(line-1, led.lines_sz); ++i) move_down();
    led.cur.sel = led.cur.cur;
}

static void _goto_start_of_selection(void) {
    selection_t sel = get_selection();
    if (led.cur.cur == sel.start) return;
    while (led.cur.cur > sel.start) move_left();
}

void remove_selection(void) {
    if (led.is_readonly) return;
    if (!is_selecting()) {
        led.cur.cur = led.lines[led.cur.line].start;
        led.cur.sel = led.lines[led.cur.line].end;
    }
    selection_t sel = get_selection();
    _goto_start_of_selection();
    // make sure it doesn't try to delete text out of bounds
    if (sel.end >= led.text_sz-1) sel.end = led.text_sz-2;
    remove_text(FALSE, (sel.end-sel.start)+1);
}

void copy_selection(void) {
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
    FILE *file = fopen("/tmp/ledsel", "w");
    if (!file) return;
    fwrite(led.text+sel.start, 1, sel.end-sel.start+1, file);
    fclose(file);
    if (system("cat '/tmp/ledsel' | xsel -b"));
}

void paste_text(void) {
    if (led.is_readonly) return;
    if (is_selecting()) remove_selection();
    // a bit roundabout, but probably still faster than pasting
    // from terminal and inserting the text character by character
    if (system("xsel -bo > /tmp/ledsel"));
    int sz;
    FILE *file = fopen("/tmp/ledsel", "r");
    if (!file) return;
    fseek(file, 0, SEEK_END);
    char *buf = malloc((sz = ftell(file)) * sizeof(char));
    fseek(file, 0, SEEK_SET);
    if (fread(buf, sizeof(char), sz, file));
    fclose(file);
    insert_text(buf, sz);
    free(buf);
}

static inline selection_t _get_selection_line_range(void) {
    selection_t lines = {0};
    selection_t sel = get_selection();
    _goto_start_of_selection();
    for (int i = led.cur.line; i < led.lines_sz; ++i) {
        const line_t *line = &led.lines[i];
        if (sel.start >= line->start && sel.start <= line->end)
            lines.start = i;
        if (sel.end >= line->start && sel.end <= line->end) {
            lines.end = i;
            break;
        }
    }
    return lines;
}

static inline void _operate_on_lines(void (*fn)(void)) {
    selection_t range = _get_selection_line_range();
    for (; led.cur.line < range.end; move_down()) fn();
}

void indent_selection(void) {
    _operate_on_lines(indent);
}

void unindent_selection(void) {
    _operate_on_lines(unindent);
}

static inline bool _is_selected(int i) {
    selection_t sel = get_selection();
    return (is_selecting() && i >= sel.start && i <= sel.end);
}

static void _render_line(int l, int off, int lineoff) {
    line_t line = led.lines[l];
    int sz = off;
    for (int i = line.start; i <= line.end; ++i) {
        int attr = 0;
        if (led.cur.cur == i) {
            led.cur_x = sz;
            led.cur_y = l-led.cur.off;
        }
        if (_is_selected(i)) attr = A_REVERSE;
        attron(attr);
        mvprintw(l-led.cur.off, sz, "%c", isspace(led.text[i])? ' ':led.text[i]);
        if (led.text[i] == '\t') {
            if (_is_selected(i)) {
                for (int j = 0; j < CFG_TABWIDTH; ++j)
                    mvprintw(l-led.cur.off, sz+j, " ");
            }
            sz += CFG_TABWIDTH;
        } else sz++;
        attroff(attr);
    }
    if (CFG_LINENUMBER) mvprintw(l-led.cur.off, 0, " %*d ", lineoff, l+1);
}

static inline int _calculate_line_size(void) {
    int sz = 0;
    for (int i = led.lines[led.cur.line].start; i < led.cur.cur; ++i)
        sz += (led.text[i] == '\t')? CFG_TABWIDTH : 1;
    return sz;
}

static void _render_text(void) {
    erase();
    int cur_off = _calculate_line_size(), lineoff = 0, off = 0;
    if (CFG_LINENUMBER) {
        char linenu[16];
        sprintf(linenu, "%ld", led.lines_sz);
        off = 2+(lineoff = strlen(linenu));
    }
    if (cur_off+off > led.ww-2) off = (led.ww-2)-cur_off;
    for (int i = led.cur.off; i < led.cur.off+led.wh-1; ++i) {
        if (i >= led.lines_sz) break;
        _render_line(i, off, lineoff);
    }
}

static inline char *_mode_to_cstr(void) {
    switch (led.mode) {
    case MODE_EXIT: return "File has been modified, exit anyway (y/n)? ";
    case MODE_FIND: return "Find: ";
    case MODE_GOTO: return "Goto: ";
    case MODE_OPEN: return "Open: ";
    case MODE_REPLACE: {
        static char str[ALLOC_SIZE] = {0};
        sprintf(str, "Replace(%.*s): ", led.input_find.text_sz, led.input_find.text);
        return str;
    }
    default: return "None";
    }
}

static void _render_status(void) {
    char status[ALLOC_SIZE] = {0};
    int attr = A_NORMAL;
#if CFG_INVERTSTATUS
    attr = A_REVERSE;
    attron(attr);
    memset(status, ' ', led.ww);
    mvprintw(led.wh-1, 0, "%s", status);
    attroff(attr);
#endif
    sprintf(status, " %s %d %d:%ld %s ",
        led.is_readonly? "[RO]" : "",
        led.cur.cur-led.lines[led.cur.line].start+1,
        led.cur.line+1, led.lines_sz, led.file);
    attron(attr);
    mvprintw(led.wh-1, led.ww-strlen(status), "%s", status);
    if (led.mode != MODE_NONE) {
        char *astr = _mode_to_cstr();
        mvprintw(led.wh-1, 0, "%s", astr);
        input_render(&led.input, strlen(astr), led.wh-1, led.ww-strlen(astr), CFG_INVERTSTATUS? A_NORMAL : A_REVERSE);
    }
    attroff(attr);
}

static void _find_next(inputbox_t input) {
    char *text = strndup(input.text, input.text_sz);
    find_string(text);
    free(text);
}

static void _replace_current(void) {
    char *to_replace = strndup(led.input_find.text, led.input_find.text_sz);
    char *replace_with = strndup(led.input.text, led.input.text_sz);
    replace_string(to_replace, replace_with);
    free(to_replace);
    free(replace_with);
}

static void _jump_to_line(void) {
    char text[INPUTBOX_TEXT_SIZE] = {0};
    memcpy(text, led.input.text, led.input.text_sz);
    long line = strtol(text, NULL, 0);
    goto_line(line);
}

static bool _update_none(int ch) {
    if (ch == CTRL('c') || ch == CTRL('q')) {
        led.mode = MODE_NONE;
        return TRUE;
    }
    return FALSE;
}

static void _update_replace(int ch) {
    if (_update_none(ch)) {
        led.input = led.input_find;
        return;
    } else if (ch == '\n' || ch == CTRL('r')) {
        if (led.input_find.text_sz) {
            _replace_current();
            _find_next(led.input_find);
        }
        return;
    } else if (ch == CTRL('f')) {
        led.input = led.input_find;
        led.mode = MODE_FIND;
        return;
    } else if (ch == CTRL('n')) {
        if (led.input.text_sz && led.input_find.text_sz)
            _find_next(led.input_find);
    }
    input_update(&led.input, ch);
}

static void _update_exit(int ch) {
    if (strchr("Yy\n", ch) || ch == CTRL('q')) exit_program();
    led.mode = MODE_NONE;
    return;
}

static void _update_find(int ch) {
    if (_update_none(ch)) return;
    if (ch == '\n' || ch == CTRL('f') || ch == CTRL('n')) {
        if (led.input.text_sz) _find_next(led.input);
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
    if (_update_none(ch)) return; \
    if (ch == '\n') { \
        if (led.input.text_sz) { BODY; } \
        led.mode = MODE_NONE; return; \
    } \
    input_update(&led.input, ch); \
}

FUPDATE(_update_goto, _jump_to_line())
FUPDATE(_update_open, open_file(strndup(led.input.text, led.input.text_sz), led.is_readonly))

#undef FUPDATE

static void _update_insert(int ch) {
    // XXX: configurable keys
    switch (ch) {
#ifdef _USE_MTM
    case 197:
        switch (getch()) {
        default: break;
        case 144: return move_down();
        case 145: return move_up();
        }
        break;
    case 198:
        switch (getch()) {
        default: break;
        case 130: return move_end();
        case 135: return move_home();
        case 137: return move_left();
        case 140: return page_down();
        case 142: return page_up();
        case 146: return move_right();
        }
        break;
    case 200:
        switch (getch()) {
        default: break;
        case 144: remove_next_word(); break;
        case 155: goto_line(led.lines_sz); break;
        case 160: goto_line(1); break;
        case 170: move_prev_word(); break;
        case 171: return move_prev_word();
        case 185: move_next_word(); break;
        case 186: return move_next_word();
        }
        break;
#endif
    case CTRL('q'):
        if (led.last_change != led.action) {
            led.mode = MODE_EXIT;
            input_reset(&led.input);
        } else {
            exit_program();
        }
        break;
    case CTRL('s'):
        write_file(led.file);
        break;
    case CTRL('c'):
        copy_selection();
        break;
    case CTRL('v'):
        paste_text();
        break;
    case CTRL('x'):
        copy_selection();
        remove_selection();
        break;
    case CTRL('g'):
        led.mode = MODE_GOTO;
        input_reset(&led.input);
        break;
    case CTRL('o'):
        led.mode = MODE_OPEN;
        input_reset(&led.input);
        break;
    case CTRL('r'): case CTRL('f'):
        led.mode = MODE_FIND;
        input_reset(&led.input);
        break;
    case CTRL('n'):
        if (led.input.text_sz) _find_next(led.input);
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
    case 547: case 544: // ctrl + home
        goto_line(1); break;
    case 542: case 539: case 334: // ctrl + end
        goto_line(led.lines_sz); break;
    case KEY_PPAGE: // pageup
        page_up(); break;
    case KEY_SPREVIOUS: // shift + pageup
        return page_up();
    case KEY_NPAGE: // pagedown
        page_down(); break;
    case KEY_SNEXT: // shift + pagedown
        return page_down();
    case '\n':
        if (is_selecting()) remove_selection();
        insert_char('\n'); break;
    case '\t':
        if (is_selecting()) indent_selection();
        indent(); break;
    case KEY_BTAB:
        if (is_selecting()) unindent_selection();
        unindent(); break;
    case KEY_DC:
        if (is_selecting()) remove_selection();
        else remove_char(FALSE);
        break;
    case KEY_BACKSPACE:
        if (is_selecting()) remove_selection();
        else {
            if (led.cur.cur == 0) break;
            remove_char(TRUE);
        }
        break;
    case 528: case 531: case 333: // ctrl + del
        if (is_selecting()) remove_selection();
        else remove_next_word();
        break;
    case 8: case 127: // ctrl + backspace
        if (is_selecting()) remove_selection();
        else remove_prev_word();
        break;
    default:
        if (!isprint(ch)) break;
        if (is_selecting()) remove_selection();
        insert_char(ch);
        break;
    }
    led.cur.sel = led.cur.cur;
}

static void _update(void) {
    void (*update_fns[])(int) = {
        [MODE_NONE]    = &_update_insert,
        [MODE_EXIT]    = &_update_exit,
        [MODE_FIND]    = &_update_find,
        [MODE_GOTO]    = &_update_goto,
        [MODE_OPEN]    = &_update_open,
        [MODE_REPLACE] = &_update_replace,
    };
    int ch = getch();
    if (ch == KEY_RESIZE) {
        getmaxyx(stdscr, led.wh, led.ww);
        while (led.cur.line-led.cur.off < 0) move_down();
        while (led.cur.line-led.cur.off >= led.wh-1) move_up();
        led.cur.sel = led.cur.cur;
    } else update_fns[led.mode](ch);
}

static void _usage(char *prg, bool extended) {
    fprintf(stderr, "usage: %s [-r|-h] <file>\n", prg);
    if (!extended) goto e;
    fprintf(stderr, "    -h    show this help and exit\n");
    fprintf(stderr, "    -r    open in read-only mode\n");
e:  exit(0);
}

int main(int argc, char **argv) {
    if (argc < 2) _usage(argv[0], FALSE);
    char *path = NULL;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-h")) _usage(argv[0], TRUE);
        else if (!strcmp(argv[i], "-r")) led.is_readonly = TRUE;
        else path = argv[i];
    }
    if (!path) _usage(argv[0], FALSE);
    open_file(strdup(path), led.is_readonly);
    _init_curses();
    for (;;) {
        curs_set(0);
        _render_text();
        _render_status();
        if (!is_selecting()) curs_set(1);
        move(led.cur_y, led.cur_x);
        _update();
    }
    _quit_curses();
    return 0;
}
