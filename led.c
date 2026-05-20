#define _GNU_SOURCE
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ncurses.h>
#include "led.h"
#include "config.h"
#define INPUTBOX_IMPL
#include "inputbox.h"
#define FILEPICKER_IMPL
#include "filepicker.h"

static struct {
    int ignore_case, show_numbers, expand_tabs, tab_width, is_readonly;
} opts;

static struct {
    int mode, ww, wh, num_buffers, max_buffers, rulersz;
    struct buffer *buffers, *cur_buffer;
    struct inputbox input, input_find;
    struct filepicker picker;
    char cwd[PATH_MAX];
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
        if (_is_action_repeat(buf, act, type)) {
            if (type == ACTION_BACKSPACE) act->cur = buf->cur;
            _insert_to_action(act, text, sz);
            return;
        }
    }
    struct action act = { .type = type, .cur = buf->cur, .text_sz = 0 };
    act.text = malloc(sizeof(char) * (act.text_cap = sz+ALLOC_SIZE));
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

static inline void _center_line(struct buffer *buf) {
    const int off = 1 + buf->cur.line - led.wh / 2;
    if (off > 0) buf->cur.off = off;
    else buf->cur.off = 0;
}

static inline void _goto_action_pos(struct buffer *buf, struct action *act) {
    buf->cur = act->cur;
    if (buf->cur.line-buf->cur.off < 0 || buf->cur.line-buf->cur.off >= led.wh-1)
        _center_line(buf);
}

void undo_action(struct buffer *buf) {
    if (buf->action == -1 || buf->is_readonly) return;
    struct action *act = &buf->actions[buf->action--];
    _goto_action_pos(buf, act);
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
    _goto_action_pos(buf, act);
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
    int line_start = buf->lines_sz = 0;
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
    if (opts.show_numbers) {
        char ln[16];
        led.rulersz = snprintf(ln, sizeof(ln), "%d", buf->lines_sz);
    }
}

static void _keepup_cursor(struct buffer *buf) {
    buf->cur.sel = buf->cur.cur; /* sometimes the cursor just has to keep up */
    if (led.mode != MODE_FIND && led.mode != MODE_REPLACE)
        buf->search_range = (struct line) { .start = 0, .end = buf->text_sz };
}

static void _init_curses(void) {
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    define_key("\033[1~", KEY_HOME);
    define_key("\033[4~", KEY_END);
#ifdef _USE_MTM
    char key[10] = {0};
    sprintf(key, "%c%c", 197, 144); define_key(key, KEY_SF);
    sprintf(key, "%c%c", 197, 145); define_key(key, KEY_SR);
    sprintf(key, "%c%c", 198, 130); define_key(key, KEY_SEND);
    sprintf(key, "%c%c", 198, 135); define_key(key, KEY_SHOME);
    sprintf(key, "%c%c", 198, 137); define_key(key, KEY_SLEFT);
    sprintf(key, "%c%c", 198, 146); define_key(key, KEY_SRIGHT);
    sprintf(key, "%c%c", 198, 140); define_key(key, KEY_SNEXT);
    sprintf(key, "%c%c", 198, 142); define_key(key, KEY_SPREVIOUS);
    sprintf(key, "%c%c", 200, 144); define_key(key, 528); /* kDC5 */
    sprintf(key, "%c%c", 200, 155); define_key(key, 539); /* kEND5 */
    sprintf(key, "%c%c", 200, 160); define_key(key, 544); /* kHOM5 */
    sprintf(key, "%c%c", 200, 170); define_key(key, 554); /* kLFT5 */
    sprintf(key, "%c%c", 200, 171); define_key(key, 555); /* kLFT6 */
    sprintf(key, "%c%c", 200, 185); define_key(key, 569); /* kRIT5 */
    sprintf(key, "%c%c", 200, 186); define_key(key, 570); /* kRIT6 */
#endif
}

static void _quit_curses(void) {
    endwin();
    curs_set(1);
}

struct buffer *create_buffer(char *name) {
    if (led.num_buffers >= led.max_buffers)
        led.buffers = realloc(led.buffers, (led.max_buffers *= 1.5)*sizeof(struct buffer));
    struct buffer *buf = &led.buffers[led.num_buffers++];
    buf->is_undo = buf->is_readonly = FALSE, buf->action = buf->last_change = -1;
    buf->text = NULL, buf->name = name, buf->cur = (struct cursor) {0};
    buf->lines = malloc((buf->lines_cap = ALLOC_SIZE)*sizeof(struct line));
    buf->actions = malloc((buf->actions_cap = ALLOC_SIZE)*sizeof(struct action));
    buf->text_sz = buf->text_cap = buf->lines_sz = buf->actions_sz = 0;
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
    if (led.cur_buffer == &led.buffers[led.num_buffers]) switch_buffer(led.cur_buffer-1);
}

void exit_program(void) {
    for (led.cur_buffer = led.buffers; led.num_buffers != 0;) close_buffer(led.cur_buffer);
    _quit_curses();
    exit(0);
}

void open_file(char *path, bool is_readonly) {
    struct stat stbuf;
    FILE *file = NULL;
    struct buffer *buf = led.cur_buffer = create_buffer(path);
    led.mode = MODE_NONE, buf->is_readonly = is_readonly;
    if (!path || stat(path, &stbuf) != 0) {
        buf->text_sz = 0, buf->text = malloc(buf->text_cap = ALLOC_SIZE);
        _count_lines(buf);
        return;
    }
    if (!S_ISREG(stbuf.st_mode) || !(stbuf.st_mode & S_IWUSR)) buf->is_readonly = TRUE;
    if (!(file = fopen(path, "r"))) goto open_file_fail;
    fseek(file, 0, SEEK_END);
    buf->text_cap = ALLOC_SIZE+(buf->text_sz = ftell(file));
    rewind(file);
    buf->text = calloc(1, buf->text_cap);
    if (!buf->text) goto open_file_fail;
    if ((int)fread(buf->text, 1, buf->text_sz, file) != buf->text_sz) goto open_file_fail;
    _count_lines(buf);
    buf->search_range = (struct line) { .start = 0, .end = buf->text_sz };
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
    FILE *file = fopen(path, "w+");
    if (!file) {
        buf->is_readonly = TRUE;
        return;
    }
    fwrite(buf->text, 1, buf->text_sz, file);
    fclose(file);
    char *full = realpath(path, NULL);
    if (full) {
        if (buf->name) free(buf->name);
        buf->name = full;
    }
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

/* XXX: UTF-8 lmao */
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
    if (is_selecting(buf)) {
        remove_selection(buf);
        return;
    }
    if (buf->cur.cur == 0) return;
    if (DELIM(buf->text[buf->cur.cur])) move_left(buf);
    buf->cur.sel = buf->cur.cur;
    move_prev_word(buf);
    remove_selection(buf);
}

void remove_next_word(struct buffer *buf) {
    if (is_selecting(buf)) {
        remove_selection(buf);
        return;
    }
    move_next_word(buf);
    remove_selection(buf);
}

void indent_line(struct buffer *buf) {
    if (buf->is_readonly) return;
    int cur = buf->cur.cur, add = 1;
    buf->cur.cur = buf->lines[buf->cur.line].start;
    /* XXX: some filetype detection would be pretty handy later on */
    if (!opts.expand_tabs || strcasestr(buf->name, "makefile")) insert_char(buf, '\t');
    else for (add = 0; add < opts.tab_width; ++add) insert_char(buf, ' ');
    buf->cur.cur = cur + add;
    if (buf->cur.cur > buf->lines[buf->cur.line].end) buf->cur.cur = buf->lines[buf->cur.line].end;
    if (buf->cur.cur < buf->lines[buf->cur.line].start) buf->cur.cur = buf->lines[buf->cur.line].start;
}

void unindent_line(struct buffer *buf) {
    if (buf->is_readonly) return;
    int cur = buf->cur.cur, rem = 1;
    buf->cur.cur = buf->lines[buf->cur.line].start;
    if (buf->text[buf->cur.cur] == '\t') remove_char(buf, FALSE);
    else for (rem = 0; rem < opts.tab_width && buf->text[buf->cur.cur] == ' '; ++rem) remove_char(buf, FALSE);
    buf->cur.cur = cur - rem;
    if (buf->cur.cur > buf->lines[buf->cur.line].end) buf->cur.cur = buf->lines[buf->cur.line].end;
    if (buf->cur.cur < buf->lines[buf->cur.line].start) buf->cur.cur = buf->lines[buf->cur.line].start;
}

static char *_casestrstr(const char *haystack, const char *needle) {
    return (opts.ignore_case)? strcasestr(haystack, needle) : strstr(haystack, needle);
}

void find_string(struct buffer *buf, char *to_find) {
    char *str = NULL, c = buf->text[buf->search_range.end+1];
    buf->text[buf->search_range.end+1] = 0;
    if ((str = _casestrstr(buf->text+buf->cur.cur+1, to_find))) {
        while (buf->text+buf->cur.cur != str && buf->cur.cur < buf->search_range.end) move_right(buf);
    } else if ((str = _casestrstr(buf->text+buf->search_range.start, to_find))) {
        while (buf->cur.cur > buf->search_range.start) move_left(buf);
        while (buf->text+buf->cur.cur != str && buf->cur.cur < buf->search_range.end) move_right(buf);
    } else goto end;
    buf->cur.sel = buf->cur.cur, buf->cur.cur += strlen(to_find)-1;
    _center_line(buf);
end:
    buf->text[buf->search_range.end+1] = c;
}

void replace_string(struct buffer *buf, char *to_replace, char *str) {
    int m = MIN(buf->cur.cur, buf->cur.sel);
    int rep_sz = strlen(to_replace), str_sz = strlen(str);
    if (_casestrstr(buf->text+m, to_replace) == buf->text+m) {
        buf->cur.cur = m;
        remove_text(buf, FALSE, rep_sz);
        buf->search_range.end -= rep_sz;
        if (str_sz) {
            insert_text(buf, str, str_sz);
            buf->search_range.end += str_sz;
        }
    }
}

void goto_line(struct buffer *buf, long line) {
    if (line <= 0) return;
    line = MIN(line-1, buf->lines_sz-1);
    buf->cur.off = (buf->cur.line = line) - led.wh/2, buf->cur.cur = buf->lines[line].start;
    _center_line(buf);
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
    remove_text(buf, FALSE, MIN(sel.end, buf->text_sz-2) - sel.start + 1);
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
#ifdef _USE_XSEL
    if (system("cat '/tmp/ledsel' | xsel -b 2> /dev/null")){}
#endif
}

void paste_text(struct buffer *buf) {
    if (buf->is_readonly) return;
    if (is_selecting(buf)) remove_selection(buf);
#ifdef _USE_XSEL
    if (system("xsel -bo 2> /dev/null > /tmp/ledsel")){}
#endif
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

static inline void _operate_on_lines(struct buffer *buf, void (*fn)(struct buffer*)) {
    struct line sel = get_selection(buf);
    _goto_start_of_selection(buf);
    for (buf->cur.sel = sel.end; buf->cur.line < buf->lines_sz; move_down(buf)) {
        const struct line *line = &buf->lines[buf->cur.line];
        const int prev = line->end - line->start;
        fn(buf);
        const int diff = (line->end - line->start) - prev;
        buf->cur.sel += diff, buf->search_range.end += diff;
        if (buf->cur.sel >= line->start && buf->cur.sel <= line->end)
            break;
    }
    /* XXX: move cursor to where it was before instead of beginning of selection */
    while (buf->cur.cur > sel.start) move_left(buf);
}

void indent_selection(struct buffer *buf) {
    if (is_selecting(buf)) _operate_on_lines(buf, indent_line);
    else { indent_line(buf); _keepup_cursor(buf); }
}

void unindent_selection(struct buffer *buf) {
    if (is_selecting(buf)) _operate_on_lines(buf, unindent_line);
    else { unindent_line(buf); _keepup_cursor(buf); }
}

static inline bool _is_selected(struct buffer *buf, int i) {
    struct line sel = get_selection(buf);
    return (is_selecting(buf) && i >= sel.start && i <= sel.end);
}

static void _render_line(struct buffer *buf, int l, int off) {
    struct line line = buf->lines[l];
    int sz = off, attr;
    for (int i = line.start; i <= line.end; ++i) {
        if (buf->cur.cur == i) buf->cur_x = sz, buf->cur_y = l-buf->cur.off;
        attr = (_is_selected(buf, i))? CFG_ATTRSELECT : A_NORMAL;
        if (i < buf->search_range.start || i > buf->search_range.end) attr |= CFG_ATTRUNSELECT;
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
    if (opts.show_numbers) {
        attr = (l == buf->cur.line)? CFG_ATTRCURLINE : CFG_ATTRLINENO;
        attron(attr);
        mvprintw(l-buf->cur.off, 0, " %*d ", led.rulersz, l+1);
        attroff(attr);
    }
}

static inline int _calculate_line_size(struct buffer *buf) {
    int sz = 0;
    for (int i = buf->lines[buf->cur.line].start; i < buf->cur.cur; ++i)
        sz += (buf->text[i] == '\t')? opts.tab_width : 1;
    return sz;
}

static void _render_text(struct buffer *buf) {
    if (led.mode >= MODE_PICKER) {
        picker_render(&led.picker);
        return;
    }
    int cur_off = _calculate_line_size(buf), off = led.rulersz? led.rulersz+2 : 0;
    if (cur_off+off > led.ww-2) off = (led.ww-2)-cur_off;
    for (int i = buf->cur.off; i < buf->cur.off+led.wh-1; ++i) {
        if (i >= buf->lines_sz) break;
        _render_line(buf, i, off);
    }
}

static inline char *_mode_to_cstr(void) {
    static char str[ALLOC_SIZE] = {0};
    switch (led.mode) {
    case MODE_EXIT: return "File has been modified, close anyway? (y/n)";
    case MODE_FIND: return "Find: ";
    case MODE_GOTO: return "Goto: ";
    case MODE_REPLACE: {
        snprintf(str, ALLOC_SIZE, "Replace(%.*s): ", led.input_find.text_sz, led.input_find.text);
        return str;
    }
    default: return "None";
    }
}

char *get_filename(const char *name, int fmt_type) {
    char *path = calloc(1, PATH_MAX), *str;
    switch (fmt_type) {
    default: strcat(path, name); break;
    case STATUS_FILENAME:
        for (str = (char*)name+strlen(name)-1; str > name && *str != '/'; --str);
        strcat(path, str + ((*str == '/')? 1 : 0));
        break;
    case STATUS_FILEPATH:
        if (strstr(name, str = getenv("HOME")) != NULL) {
            strcat(path, "~");
            strcat(path, name+strlen(str));
        } else strcat(path, name);
        break;
    }
    return path;
}

static void _render_status(void) {
    char status[ALLOC_SIZE] = {0}, *name;
    struct buffer *buf = led.cur_buffer;
    int attr = CFG_ATTRSTATUS;
    attron(attr);
    memset(status, ' ', led.ww);
    mvprintw(led.wh-1, 0, "%s", status);
    if (led.mode >= MODE_PICKER) {
        name = get_filename(led.picker.path, CFG_PICKERPATH);
        snprintf(status, ALLOC_SIZE, "%d:%d (%ld:%d %s) ",
            led.picker.cur+1, led.picker.num_files,
            (buf-led.buffers)+1, led.num_buffers, name);
    } else {
        name = get_filename(buf->name, CFG_STATUSPATH);
        snprintf(status, ALLOC_SIZE, "%s %d %d:%d (%ld:%d %s%s) ",
            buf->is_readonly? " [RO]" : "",
            buf->cur.cur-buf->lines[buf->cur.line].start+1,
            buf->cur.line+1, buf->lines_sz,
            (buf-led.buffers)+1, led.num_buffers, name,
            buf->last_change != buf->action? "*" : "");
    }
    mvprintw(led.wh-1, led.ww-strlen(status), "%s", status);
    if (led.mode != MODE_NONE) {
        struct inputbox *inp = led.mode >= MODE_PICKER? &led.picker.input : &led.input;
        const char *astr = led.mode >= MODE_PICKER? "Find: " : _mode_to_cstr();
        if (led.mode >= MODE_PICKER && !led.picker.is_searching) goto end;
        mvprintw(led.wh-1, 0, "%s", astr);
        const int s = strlen(astr), cap = s+strlen(status), w = cap+5 > led.ww? led.ww-s : led.ww-cap;
        if (led.mode != MODE_EXIT) input_render(inp, strlen(astr), led.wh-1, w, (attr&A_REVERSE)?A_NORMAL:A_REVERSE);
    }
end:
    free(name);
    attroff(attr);
}

static void _find_next(struct buffer *buf, struct inputbox input) {
    find_string(buf, input.text);
}

static void _replace_current(struct buffer *buf) {
    replace_string(buf, led.input_find.text, led.input.text);
}

static void _jump_to_line(struct buffer *buf) {
    goto_line(buf, strtol(led.input.text, NULL, 0));
    buf->cur.sel = buf->cur.cur;
}

void switch_buffer(struct buffer *buf) {
    _count_lines(led.cur_buffer = buf);
}

void next_buffer(void) {
    switch_buffer(led.cur_buffer == &led.buffers[led.num_buffers-1]? &led.buffers[0] : led.cur_buffer+1);
}

static void _list_buffers(void) {
    picker_reset(&led.picker);
    strcpy(led.picker.path, "*BUFFERS*");
    for (int i = 0; i < led.num_buffers; ++i) {
        led.picker.files[led.picker.num_files++] = (struct filepicker_entry) {
            .name = strdup(led.buffers[i].name),
            .is_dir = 0,
        };
    }
    led.picker.cur = led.cur_buffer - led.buffers;
}

void switch_mode(struct buffer *buf, int mode) {
    /* i just like writing goofy code sometimes ok */
    buf->search_range = (mode == MODE_FIND && buf->cur.sel != buf->cur.cur)
                      ? get_selection(buf)
                      : (struct line) { .start = 0, .end = buf->text_sz };
    switch (led.mode = mode) {
    case MODE_BUFFERS: _list_buffers(); return;
    case MODE_PICKER:  picker_scan(&led.picker, led.cwd); return;
    default: input_reset(&led.input); break;
    }
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
    } else if ((ch == '\n' || ch == CTRL('r')) && led.input_find.text_sz) {
        _replace_current(buf);
        _find_next(buf, led.input_find);
    } else if (ch == CTRL('f')) led.input = led.input_find, led.mode = MODE_FIND;
    else if (ch == CTRL('n') && led.input_find.text_sz) _find_next(buf, led.input_find);
    else input_update(&led.input, ch);
}

static void _update_exit(struct buffer *buf, int ch) {
    if (strchr("Yy\n", ch) || ch == CTRL('q')) close_buffer(buf);
    led.mode = MODE_NONE;
    return;
}

static void _update_find(struct buffer *buf, int ch) {
    if (_update_none(ch)) return;
    if ((ch == '\n' || ch == CTRL('f') || ch == CTRL('n')) && led.input.text_sz)
        _find_next(buf, led.input);
    else if (ch == CTRL('r') && led.input.text_sz) {
        led.mode = MODE_REPLACE, led.input_find = led.input;
        input_reset(&led.input);
    } else input_update(&led.input, ch);
}

static void _update_goto(struct buffer *buf, int ch) {
    if (_update_none(ch)) return;
    if (ch == '\n') {
        if (led.input.text_sz) _jump_to_line(buf);
        led.mode = MODE_NONE;
    } else input_update(&led.input, ch);
}

static void _update_picker(struct buffer *buf, int ch) {
    (void)buf;
    if (led.picker.num_files <= 0) {
        led.mode = MODE_NONE;
        return;
    }
    if (!led.picker.is_searching && _update_none(ch)) return;
    if (!led.picker.is_searching && ch == '\n') {
        led.mode = MODE_NONE;
        char name[PATH_MAX];
        struct filepicker_entry *sel_file = &led.picker.files[led.picker.cur];
        snprintf(name, PATH_MAX, "%s/%s", led.picker.path, sel_file->name);
        char *path = realpath(name, NULL);
        if (sel_file->is_dir) {
            if (picker_scan(&led.picker, path) > 0) led.mode = MODE_PICKER;
            free(path); /* free path here because it's not a buffer name */
            return;
        }
        for (int i = 0; i < led.num_buffers; ++i) {
            if (!strcmp(led.buffers[i].name, path)) {
                switch_buffer(&led.buffers[i]);
                return;
            }
        }
        open_file(path, opts.is_readonly);
        return;
    }
    picker_update(&led.picker, ch);
}

static void _update_buffers(struct buffer *buf, int ch) {
    (void)buf;
    if (led.picker.num_files <= 0) {
        led.mode = MODE_NONE;
        return;
    }
    if (!led.picker.is_searching && _update_none(ch)) return;
    if (!led.picker.is_searching && ch == '\n') {
        led.mode = MODE_NONE;
        struct filepicker_entry *sel_buf = &led.picker.files[led.picker.cur];
        for (int i = 0; i < led.num_buffers; ++i) {
            if (!strcmp(led.buffers[i].name, sel_buf->name)) {
                switch_buffer(&led.buffers[i]);
                return;
            }
        }
    }
    picker_update(&led.picker, ch);
}

static void _update_insert(struct buffer *buf, int ch) {
    const char *key = keyname(ch);
    /* XXX: configurable keys */
    switch (ch) {
    case CTRL('q'):
        if (buf->last_change != buf->action) led.mode = MODE_EXIT;
        else close_buffer(buf);
        return;
    case CTRL('n'):
        if (led.input.text_sz) _find_next(buf, led.input);
        return;
    case CTRL('x'):
        copy_selection(buf);
        remove_selection(buf);
        break;
    case CTRL('c'):     copy_selection(buf); break;
    case CTRL('v'):     paste_text(buf); break;
    case CTRL('o'):     switch_mode(buf, MODE_PICKER); break;
    case CTRL('b'):     switch_mode(buf, MODE_BUFFERS); break;
    case CTRL('s'):     write_file(buf, buf->name); break;
    case CTRL('z'):     undo_action(buf); break;
    case CTRL('y'):     redo_action(buf); break;
    case CTRL('g'):     switch_mode(buf, MODE_GOTO); break;
    case CTRL('r'):
    case CTRL('f'):     switch_mode(buf, MODE_FIND); break;
    case CTRL('w'):     next_buffer(); return;
    case KEY_LEFT:      move_left(buf); break;
    case KEY_SLEFT:     move_left(buf); return;
    case KEY_RIGHT:     move_right(buf); break;
    case KEY_SRIGHT:    move_right(buf); return;
    case KEY_UP:        move_up(buf); break;
    case KEY_SR:        move_up(buf); return;
    case KEY_DOWN:      move_down(buf); break;
    case KEY_SF:        move_down(buf); return;
    case KEY_FIND:
    case KEY_HOME:      move_home(buf); break;
    case KEY_SHOME:     move_home(buf); return;
    case KEY_SELECT:
    case KEY_END:       move_end(buf); break;
    case KEY_SEND:      move_end(buf); return;
    case KEY_PPAGE:     page_up(buf); break;
    case KEY_SPREVIOUS: page_up(buf); return;
    case KEY_NPAGE:     page_down(buf); break;
    case KEY_SNEXT:     page_down(buf); return;
    case '\t':          indent_selection(buf); return;
    case KEY_BTAB:      unindent_selection(buf); return;
    case 8: case 127:   remove_prev_word(buf); break;
    case KEY_DC:
        if (is_selecting(buf)) remove_selection(buf);
        else remove_char(buf, FALSE);
        break;
    case KEY_BACKSPACE:
        if (is_selecting(buf)) remove_selection(buf);
        else if (buf->cur.cur != 0) remove_char(buf, TRUE);
        break;
    default:
        if (!strcmp(key, "kDC5"))       remove_next_word(buf);
        else if (!strcmp(key, "kUP5"))  move_up(buf);
        else if (!strcmp(key, "kDN5"))  move_down(buf);
        else if (!strcmp(key, "kLFT5")) move_prev_word(buf);
        else if (!strcmp(key, "kLFT6")) { move_prev_word(buf); return; }
        else if (!strcmp(key, "kRIT5")) move_next_word(buf);
        else if (!strcmp(key, "kRIT6")) { move_next_word(buf); return; }
        else if (!strcmp(key, "kHOM5")) goto_line(buf, 1);
        else if (!strcmp(key, "kEND5")) goto_line(buf, buf->lines_sz);
        else if (isprint(ch) || ch == '\n') {
            if (is_selecting(buf)) remove_selection(buf);
            insert_char(buf, ch);
        }
        break;
    }
    _keepup_cursor(buf);
}

static void _update(struct buffer *buf) {
    static void (*update_fns[])(struct buffer*, int) = {
        [MODE_EXIT]      = &_update_exit,
        [MODE_NONE]      = &_update_insert,
        [MODE_FIND]      = &_update_find,
        [MODE_REPLACE]   = &_update_replace,
        [MODE_GOTO]      = &_update_goto,
        [MODE_PICKER]    = &_update_picker,
        [MODE_BUFFERS]   = &_update_buffers,
    };
    int ch = getch();
    if (ch == KEY_RESIZE) {
        getmaxyx(stdscr, led.wh, led.ww);
        for (struct buffer *b = led.buffers; b != &led.buffers[led.num_buffers]; ++b) {
            if (b->cur.line-b->cur.off < 0 || b->cur.line-b->cur.off >= led.wh-1)
                _center_line(b);
        }
    } else update_fns[led.mode](buf, ch);
}

static void _usage(const char *prg, bool extended) {
    fprintf(stderr, "usage: %s [-CcEeLlRrhv] [-t num] [files...]\n", prg);
    if (!extended) goto e;
    fprintf(stderr, "    -C,-c    toggles case insensitive search\n");
    fprintf(stderr, "    -E,-e    toggles expanding tabs to spaces\n");
    fprintf(stderr, "    -L,-l    toggles line numbers\n");
    fprintf(stderr, "    -R,-r    toggles read-only mode\n");
    fprintf(stderr, "    -h       print this help and exit\n");
    fprintf(stderr, "    -v       print version and exit\n");
    fprintf(stderr, "    -t num   indent using 'num' spaces\n");
e:  exit(!extended);
}

int main(int argc, char **argv) {
    opts.ignore_case = CFG_IGNORECASE, opts.show_numbers = CFG_LINENUMBER;
    opts.expand_tabs = CFG_EXPANDTABS, opts.tab_width = CFG_TABWIDTH, opts.is_readonly = FALSE;
    char *opened_dir = NULL, opt;
    struct stat stat_buf;
    led.cur_buffer = led.buffers = malloc((led.max_buffers = ALLOC_SIZE)*sizeof(struct buffer));
    while ((opt = getopt(argc, argv, "cCeElLrRhvt:")) != -1) {
        /**/ if (strchr("Cc", opt)) opts.ignore_case  = opt == 'C';
        else if (strchr("Ee", opt)) opts.expand_tabs  = opt == 'E';
        else if (strchr("Ll", opt)) opts.show_numbers = opt == 'L';
        else if (strchr("Rr", opt)) opts.is_readonly  = opt == 'R';
        else if (opt == 'v') { fprintf(stdout, "%s %s\n", argv[0], VERSION); exit(0); }
        else if (opt == 't') { int i=atoi(optarg); opts.tab_width = i? i : opts.tab_width; }
        else _usage(argv[0], opt == 'h');
    }
    for (int i = optind; i < argc; ++i) {
        char *path = realpath(argv[i], NULL);
        if (path && !stat(path, &stat_buf) && !opened_dir) {
            if (S_ISDIR(stat_buf.st_mode)) {
                opened_dir = path;
                continue;
            }
        }
        if (!opened_dir) open_file(path? path : strdup(argv[i]), opts.is_readonly);
        else free(path);
    }
    if (opened_dir) chdir(opened_dir);
    getcwd(led.cwd, PATH_MAX);
    if (!led.num_buffers || opened_dir) {
        led.mode = MODE_PICKER;
        picker_scan(&led.picker, led.cwd);
    }
    _init_curses();
    getmaxyx(stdscr, led.wh, led.ww);
    if (led.ww < MIN_TERM_WIDTH || led.wh < MIN_TERM_HEIGHT) {
        _quit_curses();
        fprintf(stderr, "error: %s requires minimum terminal size of %dx%d\n",
            argv[0], MIN_TERM_WIDTH, MIN_TERM_HEIGHT);
        return 1;
    }
    for (;;) {
        erase();
        if (led.ww >= MIN_TERM_WIDTH && led.wh >= MIN_TERM_HEIGHT) {
            curs_set(0);
            _render_text(led.cur_buffer);
            _render_status();
            if (led.mode < MODE_PICKER) {
                if (!is_selecting(led.cur_buffer)) curs_set(1);
                move(led.cur_buffer->cur_y, led.cur_buffer->cur_x);
            }
        } else mvprintw(0, 0, "terminal too small");
        _update(led.cur_buffer);
        if (led.mode != MODE_PICKER && !led.num_buffers) break;
    }
    exit_program();
    return 0;
}
