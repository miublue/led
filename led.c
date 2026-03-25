#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ncurses.h>
#include "led.h"
#include "config.h"
#define INPUTBOX_IMPL
#include "inputbox.h"

// ABANDON ALL HOPE, YE WHO ENTER HERE

static struct {
    int ignore_case, show_numbers, expand_tabs, tab_width, is_readonly;
} opts;

static struct {
    char *prgname;
    int mode, ww, wh, num_buffers, max_buffers;
    struct buffer *buffers, *cur_buffer;
    struct inputbox input, input_find;
} led;

static void _actions_append(struct buffer *buf, struct action act) {
    if (buf->action+1 < buf->actions_sz) {
        for (int i = buf->action+1; i < buf->actions_sz; ++i) free(buf->actions[i].text);
        buf->actions_sz = buf->action+1;
    }
    if (++buf->actions_sz >= buf->actions_cap)
        buf->actions = realloc(buf->actions, (buf->actions_cap += ALLOC_SIZE)*sizeof(struct action));
    buf->actions[buf->action = buf->actions_sz-1] = act;
}

static void _free_actions(struct buffer *buf) {
    for (int i = 0; i < buf->actions_sz; ++i)
        if (buf->actions[i].text_cap) free(buf->actions[i].text);
    if (buf->actions) free(buf->actions);
}

static void _insert_to_action(struct action *act, char *text, int sz) {
    if (act->text_sz+sz >= act->text_cap)
        act->text = realloc(act->text, (act->text_cap += sz+ALLOC_SIZE)*sizeof(char));
    memmove(act->text+act->text_sz, text, sz);
    act->text_sz += sz;
}

static bool _is_action_repeat(struct buffer *buf, struct action *act, int type) {
    switch (type) {
    case ACTION_DELETE: return buf->cur.cur == act->cur.cur;
    case ACTION_BACKSPACE: return buf->cur.cur == act->cur.cur-1;
    default: return buf->cur.cur == act->cur.cur + act->text_sz;
    }
}

static void _append_action(struct buffer *buf, int type, char *text, int sz) {
    if (buf->action != -1 && buf->actions[buf->action].type == type) {
        struct action *act = &buf->actions[buf->action];
        bool is_repeat = _is_action_repeat(buf, act, type);
        if (is_repeat && type == ACTION_BACKSPACE) act->cur = buf->cur;
        if (is_repeat) return _insert_to_action(act, text, sz);
    }
    struct action act = { .type = type, .cur = buf->cur };
    act.text = malloc(sizeof(char) * (act.text_cap = sz+ALLOC_SIZE));
    act.text_sz = 0;
    _insert_to_action(&act, text, sz);
    _actions_append(buf, act);
}

static inline void _undo_insert(struct buffer *buf, struct action *act) {
    buf->cur.sel = (buf->cur.cur+act->text_sz)-1;
    if (buf->cur.sel == buf->cur.cur) remove_char(buf, FALSE);
    else remove_selection(buf);
}

static inline void _undo_delete(struct buffer *buf, struct action *act) {
    insert_text(buf, act->text, act->text_sz);
}

static inline void _undo_backspace(struct buffer *buf, struct action *act) {
    char text[act->text_sz+1];
    for (int i = 0; i < act->text_sz; ++i)
        text[(act->text_sz-1)-i] = act->text[i];
    insert_text(buf, text, act->text_sz);
}

void undo_action(struct buffer *buf) {
    if (buf->action == -1 || buf->is_readonly) return;
    struct action *act = &buf->actions[buf->action--];
    buf->cur = act->cur;
    buf->is_undo = TRUE;
    switch (act->type) {
    case ACTION_INSERT: _undo_insert(buf, act); break;
    case ACTION_DELETE: _undo_delete(buf, act); break;
    case ACTION_BACKSPACE: _undo_backspace(buf, act); break;
    default: break;
    }
    buf->is_undo = FALSE;
}

void redo_action(struct buffer *buf) {
    if (buf->action+1 == buf->actions_sz || buf->is_readonly) return;
    struct action *act = &buf->actions[++buf->action];
    buf->cur = act->cur;
    buf->is_undo = TRUE;
    switch (act->type) {
    case ACTION_INSERT: _undo_delete(buf, act); break;
    case ACTION_DELETE: _undo_insert(buf, act); break;
    case ACTION_BACKSPACE: _undo_insert(buf, act); break;
    default: break;
    }
    buf->is_undo = FALSE;
}

static void _append_line(struct buffer *buf, struct line line) {
    if (++buf->lines_sz >= buf->lines_cap)
        buf->lines = realloc(buf->lines, (buf->lines_cap += ALLOC_SIZE)*sizeof(struct line));
    buf->lines[buf->lines_sz-1] = line;
}

static void _count_lines(struct buffer *buf) {
    size_t line_start = buf->lines_sz = 0;
    for (int i = 0; i < buf->text_sz; ++i) {
        if (buf->text[i] == '\n') {
            _append_line(buf, (struct line) { line_start, i });
            line_start = i+1;
        }
    }
    if (!isspace(buf->text[buf->text_sz-1]) || !buf->lines_sz) {
        buf->cur.cur = buf->text_sz;
        insert_char(buf, '\n');
        buf->cur.cur = 0;
    }
}

static void _init_curses(void) {
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
}

static void _quit_curses(void) {
    endwin();
    curs_set(1);
}

struct buffer *create_buffer(char *name) {
    if (led.num_buffers >= led.max_buffers)
        led.buffers = realloc(led.buffers, (led.max_buffers *= 1.5)*sizeof(struct buffer));
    struct buffer *buf = &led.buffers[led.num_buffers++];
    buf->text = NULL;
    buf->name = name;
    buf->lines = malloc((buf->lines_cap = ALLOC_SIZE)*sizeof(struct line));
    buf->actions = malloc((buf->actions_cap = ALLOC_SIZE)*sizeof(struct action));
    buf->text_sz = buf->text_cap = buf->lines_sz = buf->actions_sz = 0;
    buf->cur = (struct cursor) {0};
    buf->is_undo = buf->is_readonly = FALSE;
    buf->action = buf->last_change = -1;
    return buf;
}

void close_buffer(struct buffer *buf) {
    if (buf->name) free(buf->name);
    if (buf->text) free(buf->text);
    if (buf->lines) free(buf->lines);
    if (buf->actions) _free_actions(buf);
    if (--led.num_buffers == 0) exit_program();
    led.mode = MODE_NONE;
    for (struct buffer *b = buf; b != &led.buffers[led.num_buffers]; ++b) *b = *(b+1);
    if (led.cur_buffer == &led.buffers[led.num_buffers]) --led.cur_buffer;
}

void exit_program(void) {
    for (led.cur_buffer = &led.buffers[0]; led.num_buffers != 0;) close_buffer(led.cur_buffer);
    _quit_curses();
    exit(0);
}

void open_file(char *path, bool is_readonly) {
    struct stat stbuf;
    FILE *file = NULL;
    led.cur_buffer = create_buffer(path);
    led.mode = MODE_NONE;
    if (stat(path, &stbuf) != 0) {
        // try creating file if it cannot stat it, exit if that also fails
        if (!(file = fopen(path, "w+"))) goto open_file_fail;
        if (stat(path, &stbuf) != 0) goto open_file_fail;
    }
    if (!S_ISREG(stbuf.st_mode)) goto open_file_fail;
    if (is_readonly || !(stbuf.st_mode & S_IWUSR)) led.cur_buffer->is_readonly = TRUE;
    if (!file) file = fopen(path, "r");
    fseek(file, 0, SEEK_END);
    led.cur_buffer->text_cap = ALLOC_SIZE+(led.cur_buffer->text_sz = ftell(file));
    rewind(file);
    led.cur_buffer->text = calloc(1, led.cur_buffer->text_cap);
    if (!led.cur_buffer->text) goto open_file_fail;
    if (fread(led.cur_buffer->text, 1, led.cur_buffer->text_sz, file) != led.cur_buffer->text_sz) goto open_file_fail;
    _count_lines(led.cur_buffer);
    fclose(file);
    return;
open_file_fail:
    if (file) fclose(file);
    fprintf(stderr, "error: could not open file '%s'\n", path);
    exit_program();
}

void write_file(struct buffer *buf, char *path) {
    if (buf->is_readonly) return;
    buf->last_change = buf->action;
    if (!path) path = buf->name;
    FILE *file = fopen(path, "w");
    fwrite(buf->text, 1, buf->text_sz, file);
    fclose(file);
}

bool is_selecting(struct buffer *buf) {
    return (buf->cur.cur != buf->cur.sel);
}

struct line get_selection(struct buffer *buf) {
    return (struct line) {
        .start = MIN(buf->cur.cur, buf->cur.sel),
        .end = MAX(buf->cur.cur, buf->cur.sel),
    };
}

void scroll_up(struct buffer *buf) {
    if (buf->cur.line-buf->cur.off < 0 && buf->cur.off > 0) --buf->cur.off;
}

void scroll_down(struct buffer *buf) {
    if (buf->cur.line-buf->cur.off > led.wh-2) ++buf->cur.off;
}

void move_left(struct buffer *buf) {
    if (buf->cur.cur == 0) return;
    if (buf->text[--buf->cur.cur] == '\n' && buf->cur.line > 0) --buf->cur.line;
    scroll_up(buf);
}

void move_right(struct buffer *buf) {
    if (buf->cur.cur >= buf->text_sz-1) return;
    if (++buf->cur.cur == buf->lines[buf->cur.line].end+1 && buf->cur.line < buf->lines_sz) ++buf->cur.line;
    scroll_down(buf);
}

void move_up(struct buffer *buf) {
    if (buf->cur.line == 0) return;
    --buf->cur.line;
    buf->cur.cur -= buf->lines[buf->cur.line].end - buf->lines[buf->cur.line].start + 1;
    if (buf->cur.cur > buf->lines[buf->cur.line].end) buf->cur.cur = buf->lines[buf->cur.line].end;
    scroll_up(buf);
}

void move_down(struct buffer *buf) {
    if (buf->cur.line >= buf->lines_sz-1) return;
    buf->cur.cur += buf->lines[buf->cur.line].end - buf->lines[buf->cur.line].start + 1;
    ++buf->cur.line;
    if (buf->cur.cur > buf->lines[buf->cur.line].end) buf->cur.cur = buf->lines[buf->cur.line].end;
    scroll_down(buf);
}

void move_home(struct buffer *buf) {
    buf->cur.cur = buf->lines[buf->cur.line].start;
}

void move_end(struct buffer *buf) {
    buf->cur.cur = buf->lines[buf->cur.line].end;
}

void page_up(struct buffer *buf) {
    for (int i = 0; i < led.wh-2; ++i) move_up(buf);
}

void page_down(struct buffer *buf) {
    for (int i = 0; i < led.wh-2; ++i) move_down(buf);
}

#define DELIM(C) (isspace(C) || !(isalnum(C) || (C) == '_'))

void move_next_word(struct buffer *buf) {
    if (buf->cur.cur+1 >= buf->text_sz) return;
    if (DELIM(buf->text[buf->cur.cur+1]))
        while (buf->cur.cur < buf->text_sz-2 && DELIM(buf->text[buf->cur.cur+1])) move_right(buf);
    else while (buf->cur.cur < buf->text_sz-2 && !DELIM(buf->text[buf->cur.cur+1])) move_right(buf);
}

void move_prev_word(struct buffer *buf) {
    if (buf->cur.cur <= 1) return;
    if (DELIM(buf->text[buf->cur.cur-1]))
        while (buf->cur.cur > 1 && DELIM(buf->text[buf->cur.cur-1])) move_left(buf);
    else while (buf->cur.cur > 1 && !DELIM(buf->text[buf->cur.cur-1])) move_left(buf);
}

// XXX: UTF-8 lmao
void insert_text(struct buffer *buf, char *text, int sz) {
    if (buf->is_readonly) return;
    if (buf->text_sz+sz >= buf->text_cap)
        buf->text = realloc(buf->text, (buf->text_cap += sz+ALLOC_SIZE)*sizeof(char));
    memmove(buf->text+buf->cur.cur+sz, buf->text+buf->cur.cur, buf->text_sz-buf->cur.cur);
    memmove(buf->text+buf->cur.cur, text, sz);
    buf->text_sz += sz;
    _count_lines(buf);
    if (!buf->is_undo) _append_action(buf, ACTION_INSERT, text, sz);
    for (int i = 0; i < sz; ++i) move_right(buf);
}

void insert_char(struct buffer *buf, char ch) {
    insert_text(buf, &ch, 1);
}

void remove_text(struct buffer *buf, bool backspace, int sz) {
    if (buf->is_readonly) return;
    if (backspace) for (int i = 0; i < sz; ++i) move_left(buf);
    if (buf->text_sz == 0 || buf->cur.cur >= buf->text_sz-1) return;
    if (!buf->is_undo) _append_action(buf, backspace? ACTION_BACKSPACE : ACTION_DELETE, buf->text+buf->cur.cur, sz);
    memmove(buf->text+buf->cur.cur, buf->text+buf->cur.cur+sz, buf->text_sz-buf->cur.cur);
    buf->text_sz -= sz;
    _count_lines(buf);
}

void remove_char(struct buffer *buf, bool backspace) {
    remove_text(buf, backspace, 1);
}

void remove_prev_word(struct buffer *buf) {
    if (buf->cur.cur == 0) return;
    if (DELIM(buf->text[buf->cur.cur])) move_left(buf);
    buf->cur.sel = buf->cur.cur;
    move_prev_word(buf);
    remove_selection(buf);
}

void remove_next_word(struct buffer *buf) {
    move_next_word(buf);
    remove_selection(buf);
}

void indent(struct buffer *buf) {
    if (buf->is_readonly) return;
    int cur = buf->cur.cur, add = 1;
    buf->cur.cur = buf->lines[buf->cur.line].start;
    // XXX: some filetype detection would be pretty handy later on
    if (!opts.expand_tabs || !strcasecmp(buf->name, "makefile"))
        insert_char(buf, '\t');
    else for (add = 0; add < opts.tab_width; ++add)
        insert_char(buf, ' ');
    buf->cur.cur = cur + add;
    if (buf->cur.cur > buf->lines[buf->cur.line].end) buf->cur.cur = buf->lines[buf->cur.line].end;
    if (buf->cur.cur < buf->lines[buf->cur.line].start) buf->cur.cur = buf->lines[buf->cur.line].start;
}

void unindent(struct buffer *buf) {
    if (buf->is_readonly) return;
    int cur = buf->cur.cur, rem = 1;
    buf->cur.cur = buf->lines[buf->cur.line].start;
    if (buf->text[buf->cur.cur] == '\t')
        remove_char(buf, FALSE);
    else for (rem = 0; rem < opts.tab_width && buf->text[buf->cur.cur] == ' '; ++rem)
        remove_char(buf, FALSE);
    buf->cur.cur = cur - rem;
    if (buf->cur.cur > buf->lines[buf->cur.line].end) buf->cur.cur = buf->lines[buf->cur.line].end;
    if (buf->cur.cur < buf->lines[buf->cur.line].start) buf->cur.cur = buf->lines[buf->cur.line].start;
}

static char *_casestrstr(const char *haystack, const char *needle) {
    if (opts.ignore_case) return strcasestr(haystack, needle);
    return strstr(haystack, needle);
}

// XXX: move line to the center of the screen on find
void find_string(struct buffer *buf, char *to_find) {
    char *str = NULL;
    if ((str = _casestrstr(buf->text+buf->cur.cur+1, to_find))) {
        while (buf->text+buf->cur.cur != str && buf->cur.cur < buf->text_sz) move_right(buf);
    } else if ((str = _casestrstr(buf->text, to_find))) {
        buf->cur = (struct cursor) {0};
        while (buf->text+buf->cur.cur != str && buf->cur.cur < buf->text_sz) move_right(buf);
    } else return;
    buf->cur.sel = buf->cur.cur;
    buf->cur.cur += strlen(to_find)-1;
}

void replace_string(struct buffer *buf, char *to_replace, char *str) {
    int m = MIN(buf->cur.cur, buf->cur.sel);
    int rep_sz = strlen(to_replace), str_sz = strlen(str);
    if (!strncmp(buf->text+m, to_replace, rep_sz)) {
        buf->cur.cur = m;
        remove_text(buf, FALSE, rep_sz);
        if (str_sz) insert_text(buf, str, str_sz);
    }
}

void goto_line(struct buffer *buf, long line) {
    if (line == 0) return;
    buf->cur.cur = buf->cur.off = buf->cur.line = 0;
    for (int i = 0; i < MIN(line-1, buf->lines_sz); ++i) move_down(buf);
    buf->cur.sel = buf->cur.cur;
}

static void _goto_start_of_selection(struct buffer *buf) {
    struct line sel = get_selection(buf);
    if (buf->cur.cur == sel.start) return;
    while (buf->cur.cur > sel.start) move_left(buf);
}

void remove_selection(struct buffer *buf) {
    if (buf->is_readonly) return;
    if (!is_selecting(buf)) {
        buf->cur.cur = buf->lines[buf->cur.line].start;
        buf->cur.sel = buf->lines[buf->cur.line].end;
    }
    struct line sel = get_selection(buf);
    _goto_start_of_selection(buf);
    // make sure it doesn't try to delete text out of bounds
    if (sel.end >= buf->text_sz-1) sel.end = buf->text_sz-2;
    remove_text(buf, FALSE, (sel.end-sel.start)+1);
}

void copy_selection(struct buffer *buf) {
    int cur = buf->cur.cur;
    bool not_sel = FALSE;
    if (!is_selecting(buf)) {
        not_sel = TRUE;
        buf->cur.cur = buf->lines[buf->cur.line].start;
        buf->cur.sel = buf->lines[buf->cur.line].end;
    }
    struct line sel = get_selection(buf);
    buf->cur.cur = cur;
    if (not_sel) buf->cur.sel = buf->cur.cur;
    FILE *file = fopen("/tmp/ledsel", "w");
    if (!file) return;
    fwrite(buf->text+sel.start, 1, sel.end-sel.start+1, file);
    fclose(file);
    if (system("cat '/tmp/ledsel' | xsel -b")){}
}

void paste_text(struct buffer *buf) {
    if (buf->is_readonly) return;
    if (is_selecting(buf)) remove_selection(buf);
    // a bit roundabout, but probably still faster than pasting
    // from terminal and inserting the text character by character
    if (system("xsel -bo > /tmp/ledsel")){}
    int sz;
    FILE *file = fopen("/tmp/ledsel", "r");
    if (!file) return;
    fseek(file, 0, SEEK_END);
    char *text = malloc((sz = ftell(file)) * sizeof(char));
    fseek(file, 0, SEEK_SET);
    if (fread(text, sizeof(char), sz, file)){}
    fclose(file);
    insert_text(buf, text, sz);
    free(text);
}

static inline struct line _get_selection_line_range(struct buffer *buf) {
    struct line lines = {0};
    struct line sel = get_selection(buf);
    _goto_start_of_selection(buf);
    for (int i = buf->cur.line; i < buf->lines_sz; ++i) {
        const struct line *line = &buf->lines[i];
        if (sel.start >= line->start && sel.start <= line->end)
            lines.start = i;
        if (sel.end >= line->start && sel.end <= line->end) {
            lines.end = i;
            break;
        }
    }
    return lines;
}

static inline void _operate_on_lines(struct buffer *buf, void (*fn)(struct buffer*)) {
    struct line range = _get_selection_line_range(buf);
    for (; buf->cur.line < range.end; move_down(buf)) fn(buf);
}

void indent_selection(struct buffer *buf) { _operate_on_lines(buf, indent); }
void unindent_selection(struct buffer *buf) { _operate_on_lines(buf, unindent); }

static inline bool _is_selected(struct buffer *buf, int i) {
    struct line sel = get_selection(buf);
    return (is_selecting(buf) && i >= sel.start && i <= sel.end);
}

static void _render_line(struct buffer *buf, int l, int off, int lineoff) {
    struct line line = buf->lines[l];
    int sz = off;
    for (int i = line.start; i <= line.end; ++i) {
        int attr = 0;
        if (buf->cur.cur == i) {
            buf->cur_x = sz;
            buf->cur_y = l-buf->cur.off;
        }
        if (_is_selected(buf, i)) attr = A_REVERSE;
        attron(attr);
        mvprintw(l-buf->cur.off, sz, "%c", isspace(buf->text[i])? ' ' : buf->text[i]);
        if (buf->text[i] == '\t') {
            if (_is_selected(buf, i)) {
                for (int j = 0; j < opts.tab_width; ++j)
                    mvprintw(l-buf->cur.off, sz+j, "%c", ' ');
            }
            sz += opts.tab_width;
        } else sz++;
        attroff(attr);
    }
    if (opts.show_numbers) mvprintw(l-buf->cur.off, 0, " %*d ", lineoff, l+1);
}

static inline int _calculate_line_size(struct buffer *buf) {
    int sz = 0;
    for (int i = buf->lines[buf->cur.line].start; i < buf->cur.cur; ++i)
        sz += (buf->text[i] == '\t')? opts.tab_width : 1;
    return sz;
}

static void _render_text(struct buffer *buf) {
    int cur_off = _calculate_line_size(buf), lineoff = 0, off = 0;
    if (opts.show_numbers) {
        char linenu[16];
        sprintf(linenu, "%ld", buf->lines_sz);
        off = 2+(lineoff = strlen(linenu));
    }
    if (cur_off+off > led.ww-2) off = (led.ww-2)-cur_off;
    for (int i = buf->cur.off; i < buf->cur.off+led.wh-1; ++i) {
        if (i >= buf->lines_sz) break;
        _render_line(buf, i, off, lineoff);
    }
}

static inline char *_mode_to_cstr(void) {
    static char str[ALLOC_SIZE] = {0};
    switch (led.mode) {
    case MODE_EXIT: return "File has been modified, close anyway? (y/n)";
    case MODE_FIND: return "Find: ";
    case MODE_GOTO: return "Goto: ";
    case MODE_OPEN: return "Open: ";
    case MODE_REPLACE: {
        sprintf(str, "Replace(%.*s): ", led.input_find.text_sz, led.input_find.text);
        return str;
    }
    default: return "None";
    }
}

static void _render_status(void) {
    char status[ALLOC_SIZE] = {0};
    struct buffer *buf = led.cur_buffer;
    int attr = A_NORMAL;
#if CFG_INVERTSTATUS
    attr = A_REVERSE;
    attron(attr);
    memset(status, ' ', led.ww);
    mvprintw(led.wh-1, 0, "%s", status);
    attroff(attr);
#endif
    sprintf(status, "%s %d %d:%ld (%ld:%d %s) ",
        buf->is_readonly? " [RO]" : "",
        buf->cur.cur-buf->lines[buf->cur.line].start+1,
        buf->cur.line+1, buf->lines_sz,
        (buf-led.buffers)+1, led.num_buffers, buf->name);
    attron(attr);
    mvprintw(led.wh-1, led.ww-strlen(status), "%s", status);
    if (led.mode != MODE_NONE) {
        const char *astr = _mode_to_cstr();
        mvprintw(led.wh-1, 0, "%s", astr);
        const int s = strlen(astr), cap = s+strlen(status), at = CFG_INVERTSTATUS? A_NORMAL : A_REVERSE;
        const int w = cap+5 > led.ww? led.ww-s : led.ww-cap;
        if (led.mode != MODE_EXIT)
            input_render(&led.input, strlen(astr), led.wh-1, w, at);
    }
    attroff(attr);
}

static void _find_next(struct buffer *buf, struct inputbox input) {
    char *text = strndup(input.text, input.text_sz);
    find_string(buf, text);
    free(text);
}

static void _replace_current(struct buffer *buf) {
    char *to_replace = strndup(led.input_find.text, led.input_find.text_sz);
    char *replace_with = strndup(led.input.text, led.input.text_sz);
    replace_string(buf, to_replace, replace_with);
    free(to_replace);
    free(replace_with);
}

static void _jump_to_line(struct buffer *buf) {
    char text[INPUTBOX_TEXT_SIZE] = {0};
    memcpy(text, led.input.text, led.input.text_sz);
    long line = strtol(text, NULL, 0);
    goto_line(buf, line);
}

static bool _update_none(int ch) {
    if (ch == CTRL('c') || ch == CTRL('q')) {
        led.mode = MODE_NONE;
        return TRUE;
    }
    return FALSE;
}

static void _update_replace(struct buffer *buf, int ch) {
    if (_update_none(ch)) {
        led.input = led.input_find;
        return;
    } else if (ch == '\n' || ch == CTRL('r')) {
        if (led.input_find.text_sz) {
            _replace_current(buf);
            _find_next(buf, led.input_find);
        }
        return;
    } else if (ch == CTRL('f')) {
        led.input = led.input_find;
        led.mode = MODE_FIND;
        return;
    } else if (ch == CTRL('n')) {
        if (led.input.text_sz && led.input_find.text_sz)
            _find_next(buf, led.input_find);
    }
    input_update(&led.input, ch);
}

static void _update_exit(struct buffer *buf, int ch) {
    if (strchr("Yy\n", ch) || ch == CTRL('q')) close_buffer(buf);
    led.mode = MODE_NONE;
    return;
}

static void _update_find(struct buffer *buf, int ch) {
    if (_update_none(ch)) return;
    if (ch == '\n' || ch == CTRL('f') || ch == CTRL('n')) {
        if (led.input.text_sz) _find_next(buf, led.input);
        return;
    } else if (ch == CTRL('r') && led.input.text_sz) {
        led.mode = MODE_REPLACE;
        led.input_find = led.input;
        input_reset(&led.input);
        return;
    }
    input_update(&led.input, ch);
}

static void _update_goto(struct buffer *buf, int ch) {
    if (_update_none(ch)) return;
    if (ch == '\n') {
        if (led.input.text_sz) _jump_to_line(buf);
        led.mode = MODE_NONE;
        return;
    }
    input_update(&led.input, ch);
}

static void _update_open(struct buffer *buf, int ch) {
    if (_update_none(ch)) return;
    if (ch == '\n') {
        led.mode = MODE_NONE;
        if (!led.input.text_sz) return;
        char *name = strndup(led.input.text, led.input.text_sz);
        for (int i = 0; i < led.num_buffers; ++i) {
            if (!strcmp(led.buffers[i].name, name)) {
                free(name);
                led.cur_buffer = &led.buffers[i];
                return;
            }
        }
        open_file(name, opts.is_readonly);
        return;
    }
    input_update(&led.input, ch);
}

static void _update_insert(struct buffer *buf, int ch) {
    // XXX: configurable keys
    switch (ch) {
#ifdef _USE_MTM
    case 197:
        switch (getch()) {
        default: break;
        case 144: return move_down(buf);
        case 145: return move_up(buf);
        }
        break;
    case 198:
        switch (getch()) {
        default: break;
        case 130: return move_end(buf);
        case 135: return move_home(buf);
        case 137: return move_left(buf);
        case 140: return page_down(buf);
        case 142: return page_up(buf);
        case 146: return move_right(buf);
        }
        break;
    case 200:
        switch (getch()) {
        default: break;
        case 144: remove_next_word(buf); break;
        case 155: goto_line(buf, buf->lines_sz); break;
        case 160: goto_line(buf, 1); break;
        case 170: move_prev_word(buf); break;
        case 171: return move_prev_word(buf);
        case 185: move_next_word(buf); break;
        case 186: return move_next_word(buf);
        }
        break;
#endif
    case CTRL('q'):
        if (buf->last_change != buf->action) led.mode = MODE_EXIT;
        else close_buffer(buf);
        return;
    case CTRL('o'):
        led.mode = MODE_OPEN;
        input_reset(&led.input);
        break;
    case CTRL('s'):
        write_file(buf, buf->name); break;
    case CTRL('z'):
        undo_action(buf); break;
    case CTRL('y'):
        redo_action(buf); break;
    case CTRL('c'):
        copy_selection(buf); break;
    case CTRL('v'):
        paste_text(buf); break;
    case CTRL('x'):
        copy_selection(buf);
        remove_selection(buf);
        break;
    case CTRL('g'):
        led.mode = MODE_GOTO;
        input_reset(&led.input);
        break;
    case CTRL('r'): case CTRL('f'):
        led.mode = MODE_FIND;
        input_reset(&led.input);
        break;
    case CTRL('n'):
        if (led.input.text_sz) _find_next(buf, led.input);
        return;
    case CTRL('w'):
        if (led.num_buffers > 1) {
            if (++led.cur_buffer == &led.buffers[led.num_buffers])
                led.cur_buffer = &led.buffers[0];
        }
        return;
    case KEY_LEFT:
        move_left(buf); break;
    case KEY_SLEFT:
        return move_left(buf);
    case KEY_RIGHT:
        move_right(buf); break;
    case KEY_SRIGHT:
        return move_right(buf);
    // XXX: these keys differ depending on the keyboard/terminal
    case 560: case 569: case 572: // ctrl + right
        move_next_word(buf); break;
    case 561: case 570: case 573: // ctrl + shift + right
        return move_next_word(buf);
    case 545: case 554: case 557: // ctrl + left
        move_prev_word(buf); break;
    case 546: case 555: case 558: // ctrl + shift + left
        return move_prev_word(buf);
    case KEY_UP:
        move_up(buf); break;
    case KEY_SR:
        return move_up(buf);
    case KEY_DOWN:
        move_down(buf); break;
    case KEY_SF:
        return move_down(buf);
    case KEY_HOME:
        move_home(buf); break;
    case KEY_SHOME:
        return move_home(buf);
    case KEY_END:
        move_end(buf); break;
    case KEY_SEND:
        return move_end(buf);
    case 547: case 544: // ctrl + home
        goto_line(buf, 1); break;
    case 542: case 539: case 334: // ctrl + end
        goto_line(buf, buf->lines_sz); break;
    case KEY_PPAGE: // pageup
        page_up(buf); break;
    case KEY_SPREVIOUS: // shift + pageup
        return page_up(buf);
    case KEY_NPAGE: // pagedown
        page_down(buf); break;
    case KEY_SNEXT: // shift + pagedown
        return page_down(buf);
    case '\n':
        if (is_selecting(buf)) remove_selection(buf);
        insert_char(buf, '\n'); break;
    case '\t':
        if (is_selecting(buf)) indent_selection(buf);
        indent(buf); break;
    case KEY_BTAB:
        if (is_selecting(buf)) unindent_selection(buf);
        unindent(buf); break;
    case KEY_DC:
        if (is_selecting(buf)) remove_selection(buf);
        else remove_char(buf, FALSE);
        break;
    case KEY_BACKSPACE:
        if (is_selecting(buf)) remove_selection(buf);
        else {
            if (buf->cur.cur == 0) break;
            remove_char(buf, TRUE);
        }
        break;
    case 528: case 531: case 333: // ctrl + del
        if (is_selecting(buf)) remove_selection(buf);
        else remove_next_word(buf);
        break;
    case 8: case 127: // ctrl + backspace
        if (is_selecting(buf)) remove_selection(buf);
        else remove_prev_word(buf);
        break;
    default: {
        if (!isprint(ch)) break;
        if (is_selecting(buf)) remove_selection(buf);
        insert_char(buf, ch);
    } break;
    }
    buf->cur.sel = buf->cur.cur;
}

static void _update(struct buffer *buf) {
    void (*update_fns[])(struct buffer*, int) = {
        [MODE_EXIT]    = &_update_exit,
        [MODE_NONE]    = &_update_insert,
        [MODE_FIND]    = &_update_find,
        [MODE_GOTO]    = &_update_goto,
        [MODE_OPEN]    = &_update_open,
        [MODE_REPLACE] = &_update_replace,
    };
    int ch = getch();
    if (ch == KEY_RESIZE) {
        getmaxyx(stdscr, led.wh, led.ww);;
        while (buf->cur.line-buf->cur.off < 0) move_down(buf);
        while (buf->cur.line-buf->cur.off >= led.wh-1) move_up(buf);
        buf->cur.sel = buf->cur.cur;
    } else update_fns[led.mode](buf, ch);
}

static void _usage(bool extended) {
    fprintf(stderr, "usage: %s [-h|-r|-c|-l|-e|-t num] file [file...]\n", led.prgname);
    if (!extended) goto e;
    fprintf(stderr, "    -h       show this help and exit\n");
    fprintf(stderr, "    -r       open in read-only mode\n");
    fprintf(stderr, "    -c       toggle search ignores case\n");
    fprintf(stderr, "    -l       toggle drawing line numbers\n");
    fprintf(stderr, "    -e       toggle expanding tabs to spaces\n");
    fprintf(stderr, "    -t num   indent using 'num' spaces\n");
e:  exit(0);
}

int main(int argc, char **argv) {
    led.prgname = argv[0];
    if (argc < 2) _usage(FALSE);
    opts.ignore_case = CFG_IGNORECASE;
    opts.show_numbers = CFG_LINENUMBER;
    opts.expand_tabs = CFG_EXPANDTAB;
    opts.tab_width = CFG_TABWIDTH;
    opts.is_readonly = FALSE;
    led.buffers = malloc((led.max_buffers = ALLOC_SIZE)*sizeof(struct buffer));
    led.cur_buffer = NULL;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-h")) _usage(TRUE);
        else if (!strcmp(argv[i], "-r")) opts.is_readonly   = TRUE;
        else if (!strcmp(argv[i], "-c")) opts.ignore_case  = !opts.ignore_case;
        else if (!strcmp(argv[i], "-l")) opts.show_numbers = !opts.show_numbers;
        else if (!strcmp(argv[i], "-e")) opts.expand_tabs  = !opts.expand_tabs;
        else if (!strcmp(argv[i], "-t") && i+1 < argc) opts.tab_width = atoi(argv[++i]);
        else if (argv[i][0] == '-') _usage(TRUE);
        else open_file(strdup(argv[i]), opts.is_readonly);
    }
    if (!led.num_buffers) _usage(FALSE);
    _init_curses();
    getmaxyx(stdscr, led.wh, led.ww);
    if (led.ww < MIN_TERM_WIDTH || led.wh < MIN_TERM_HEIGHT) {
        _quit_curses();
        fprintf(stderr, "error: %s requires minimal terminal size of %dx%d\n",
            led.prgname, MIN_TERM_WIDTH, MIN_TERM_HEIGHT);
        return 1;
    }
    for (;;) {
        if (led.ww >= MIN_TERM_WIDTH && led.wh >= MIN_TERM_HEIGHT) {
            curs_set(0);
            erase();
            _render_text(led.cur_buffer);
            _render_status();
            if (!is_selecting(led.cur_buffer)) curs_set(1);
            move(led.cur_buffer->cur_y, led.cur_buffer->cur_x);
        } else {
            erase();
            mvprintw(0, 0, "terminal too small");
        }
        _update(led.cur_buffer);
    }
    exit_program();
    return 0;
}
