#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ncurses.h>
#include "inputbox.h"
#include "led.h"
#include "config.h"
#include "callbacks.h"
#include "parser.h"

// ABANDON ALL HOPE, YE WHO ENTER HERE

static struct {
    size_t text_sz, text_alloc;
    size_t lines_sz, lines_alloc;
    size_t tokens_sz, tokens_alloc;
    size_t action, actions_sz, actions_alloc;
    int mode, readonly, ww, wh;
    cursor_t cur;
    bool is_undo;
    char *text, *file;
    line_t *lines;
    action_t *actions;
    syntax_t *syntax;
    token_t *tokens;
    inputbox_t input, input_find;
} led;

// XXX: adding new config options and languages is a pain in the ass
#include "syntax/makefile.h"
#include "syntax/markdown.h"
#include "syntax/asm.h"
#include "syntax/c.h"
#include "syntax/d.h"
#include "syntax/sh.h"
#include "syntax/python.h"
#include "syntax/lua.h"
#include "syntax/ledrc.h"
#include "syntax/nim.h"
#include "syntax/sml.h"
#include "syntax/lisp.h"
#include "syntax/scheme.h"
#include "syntax/go.h"
#include "syntax/rust.h"
#include "syntax/zig.h"

static syntax_t syntaxes[512] = {0};
static int num_syntaxes = 0;

static inline void _init_syntaxes(void) {
    num_syntaxes = 0;
    syntaxes[num_syntaxes++] = syntax_makefile();
    syntaxes[num_syntaxes++] = syntax_markdown();
    syntaxes[num_syntaxes++] = syntax_asm();
    syntaxes[num_syntaxes++] = syntax_c();
    syntaxes[num_syntaxes++] = syntax_d();
    syntaxes[num_syntaxes++] = syntax_sh();
    syntaxes[num_syntaxes++] = syntax_python();
    syntaxes[num_syntaxes++] = syntax_lua();
    syntaxes[num_syntaxes++] = syntax_ledrc();
    syntaxes[num_syntaxes++] = syntax_nim();
    syntaxes[num_syntaxes++] = syntax_sml();
    syntaxes[num_syntaxes++] = syntax_lisp();
    syntaxes[num_syntaxes++] = syntax_scheme();
    syntaxes[num_syntaxes++] = syntax_go();
    syntaxes[num_syntaxes++] = syntax_rust();
    syntaxes[num_syntaxes++] = syntax_zig();
}

// XXX: occasional segfaults when inserting text after undo
static void _actions_append(action_t act) {
    if (led.action+1 < led.actions_sz) {
        for (int i = led.action+1; i < led.actions_sz; ++i) {
            if (led.actions[i].text_alloc) free(led.actions[i].text);
            memset(&led.actions[i], 0, sizeof(action_t));
        }
        led.actions_sz = led.action+1;
    }
    if (++led.actions_sz >= led.actions_alloc)
        led.actions = realloc(led.actions, sizeof(action_t) * (led.actions_alloc += ALLOC_SIZE));
    led.actions[led.action = led.actions_sz-1] = act;
}

static void _free_actions(void) {
    for (int i = 0; i < led.actions_sz; ++i) {
        if (led.actions[i].text_alloc) free(led.actions[i].text);
    }
    if (led.actions) free(led.actions);
}

static void _insert_to_action(action_t *act, char c) {
    if (act->text_sz >= act->text_alloc)
        act->text = realloc(act->text, sizeof(char) * (act->text_alloc += ALLOC_SIZE));
    act->text[act->text_sz++] = c;
}

static bool _is_action_repeat(int type, action_t *act) {
    switch (type) {
    case ACTION_DELETE: return led.cur.cur == act->cur.cur;
    case ACTION_BACKSPACE: return led.cur.cur == act->cur.cur-1;
    default: return led.cur.cur == act->cur.cur + act->text_sz;
    }
}

static void _append_action(int type, char c) {
    if (led.action != -1 && led.actions[led.action].type == type) {
        action_t *act = &led.actions[led.action];
        bool repeat = _is_action_repeat(type, act);
        if (repeat && type == ACTION_BACKSPACE) act->cur = led.cur;
        if (repeat) return _insert_to_action(act, c);
    }
    action_t act = { .type = type, .cur = led.cur };
    act.text = malloc(sizeof(char) * (act.text_alloc = ALLOC_SIZE));
    act.text_sz = 0;
    _insert_to_action(&act, c);
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

static inline void _undo_upper_or_lower(action_t *act, int type) {
    for (int i = 0; i < act->text_sz; ++i) {
        if (type == ACTION_TOUPPER) led.text[led.cur.cur+i] = tolower(led.text[led.cur.cur+i]);
        else led.text[led.cur.cur+i] = toupper(led.text[led.cur.cur+i]);
    }
}

void undo_action(void) {
    if (led.action == -1 || led.readonly) return;
    action_t *act = &led.actions[led.action--];
    led.cur = act->cur;
    led.is_undo = TRUE;
    switch (act->type) {
    case ACTION_INSERT: _undo_insert(act); break;
    case ACTION_DELETE: _undo_delete(act); break;
    case ACTION_BACKSPACE: _undo_backspace(act); break;
    case ACTION_TOUPPER: case ACTION_TOLOWER:
        _undo_upper_or_lower(act, act->type); break;
    default: break;
    }
    led.is_undo = FALSE;
    parse_tokens();
}

void redo_action(void) {
    if (led.action+1 == led.actions_sz || led.readonly) return;
    action_t *act = &led.actions[++led.action];
    led.cur = act->cur;
    led.is_undo = TRUE;
    switch (act->type) {
    case ACTION_INSERT: _undo_delete(act); break;
    case ACTION_DELETE: _undo_insert(act); break;
    case ACTION_BACKSPACE: _undo_insert(act); break;
    case ACTION_TOUPPER: case ACTION_TOLOWER:
        _undo_upper_or_lower(act, act->type == ACTION_TOUPPER? ACTION_TOLOWER : ACTION_TOUPPER);
        break;
    default: break;
    }
    led.is_undo = FALSE;
    parse_tokens();
}

static void _append_token(token_t token) {
    if (++led.tokens_sz >= led.tokens_alloc)
        led.tokens = realloc(led.tokens, sizeof(token_t) * (led.tokens_alloc += ALLOC_SIZE));
    led.tokens[led.tokens_sz-1] = token;
}

static void _append_line(line_t line) {
    if (++led.lines_sz >= led.lines_alloc)
        led.lines = realloc(led.lines, sizeof(line_t) * (led.lines_alloc += ALLOC_SIZE));
    led.lines[led.lines_sz-1] = line;
}

void parse_tokens(void) {
    led.tokens_sz = 0;
    if (!led.syntax || !cfg_get_value_idx(CFG_HIGHLIGHT)->as_bool) return;
    token_t tok = {0};
    lexer_t lex = { .cur = 0, .text_sz = led.text_sz, .text = led.text };
    do {
        tok = led.syntax->next_token(led.syntax, &lex);
        _append_token(tok);
    } while (tok.type != LTK_EOF && lex.cur < led.text_sz);
}

void load_syntax(char *name) {
    if (name) {
        for (int i = 0; i < num_syntaxes; ++i) {
            if (!strcasecmp(name, syntaxes[i].name)) {
                led.syntax = &syntaxes[i];
                return;
            }
        }
    } else {
        for (int i = 0; i < num_syntaxes; ++i) {
            for (int j = 0; syntaxes[i].extensions[j]; ++j) {
                const char *ext = syntaxes[i].extensions[j];
                const int file_sz = strlen(led.file), ext_sz = strlen(ext);
                if (ext_sz <= file_sz && !strncmp(ext, led.file+file_sz-ext_sz, ext_sz)) {
                    led.syntax = &syntaxes[i];
                    return;
                }
            }
        }
    }
}

static void _count_lines(void) {
    size_t line_start = led.lines_sz = 0;
    parse_tokens();
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
    curs_set(0);
    keypad(stdscr, TRUE);
#if _USE_COLOR
    use_default_colors();
    start_color();
    init_pair(PAIR_NORMAL, -1, -1);
    init_pair(PAIR_STATUS,   COLOR_STATUS,   -1);
    init_pair(PAIR_LITERAL,  COLOR_LITERAL,  -1);
    init_pair(PAIR_KEYWORD,  COLOR_KEYWORD,  -1);
    init_pair(PAIR_OPERATOR, COLOR_OPERATOR, -1);
    init_pair(PAIR_COMMENT,  COLOR_COMMENT,  -1);
#endif
}

static void _quit_curses(void) {
    endwin();
    curs_set(1);
}

void exit_program(void) {
    // XXX: i'm getting double-free corruption, my solution is to leave it to the OS
/*
    if (led.file) free(led.file);
    if (led.text) free(led.text);
    if (led.lines) free(led.lines);
    if (led.tokens) free(led.tokens);
    if (led.actions) _free_actions();
*/
    _quit_curses();
    exit(0);
}

static void _load_config_file(void) {
    char path[ALLOC_SIZE] = {0};
    sprintf(path, "%s/.ledrc", getenv("HOME"));
    cfg_parse_file(path);
}

void open_file(char *path) {
    if (led.file) free(led.file);
    if (led.text) free(led.text);
    if (led.lines) free(led.lines);
    if (led.tokens) free(led.tokens);
    if (led.actions) _free_actions();
    led.text = NULL;
    led.syntax = NULL;
    led.lines = malloc(sizeof(line_t) * (led.lines_alloc = ALLOC_SIZE));
    led.tokens = malloc(sizeof(token_t) * (led.tokens_alloc = ALLOC_SIZE));
    led.actions = malloc(sizeof(action_t) * (led.actions_alloc = ALLOC_SIZE));
    led.text_sz = led.lines_sz = led.actions_sz = led.tokens_sz = 0;
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

    _init_syntaxes();
    load_syntax(NULL);
    _count_lines();
    fclose(file);
    _load_config_file();
    return;
open_file_fail:
    if (file) fclose(file);
    fprintf(stderr, "error: could not open file: %s\n", path);
    exit_program();
}

void write_file(char *path) {
    if (led.readonly) return;
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
    if (led.readonly) return;
    if ((led.text_sz += sz) >= led.text_alloc)
        led.text = realloc(led.text, (led.text_alloc += sz+ALLOC_SIZE));
    memmove(led.text+led.cur.cur+sz, led.text+led.cur.cur, led.text_sz-led.cur.cur);
    memmove(led.text+led.cur.cur, buf, sz);
    _count_lines();
    for (int i = 0; i < sz; ++i) {
        if (!led.is_undo) _append_action(ACTION_INSERT, buf[i]);
        move_right();
    }
}

void insert_char(int ch) {
    if (led.readonly) return;
    if (++led.text_sz >= led.text_alloc)
        led.text = realloc(led.text, (led.text_alloc += ALLOC_SIZE));
    if (!led.is_undo) _append_action(ACTION_INSERT, ch);
    memmove(led.text+led.cur.cur+1, led.text+led.cur.cur, led.text_sz-led.cur.cur);
    led.text[led.cur.cur] = ch;
    _count_lines();
    move_right();
}

void indent(void) {
    if (led.readonly) return;
    int cur = led.cur.cur, add = 1;
    led.cur.cur = led.lines[led.cur.line].start;
    if (!cfg_get_value_idx(CFG_EXPAND_TAB)->as_bool || (led.syntax && !strcasecmp(led.syntax->name, "makefile"))) {
        insert_char('\t');
    } else {
        for (add = 0; add < cfg_get_value_idx(CFG_TAB_WIDTH)->as_int; ++add)
            insert_char(' ');
    }
    led.cur.cur = cur + add;
    if (led.cur.cur > led.lines[led.cur.line].end) led.cur.cur = led.lines[led.cur.line].end;
    if (led.cur.cur < led.lines[led.cur.line].start) led.cur.cur = led.lines[led.cur.line].start;
}

void unindent(void) {
    if (led.readonly) return;
    int cur = led.cur.cur, rem = 1;
    led.cur.cur = led.lines[led.cur.line].start;
    if (led.text[led.cur.cur] == '\t') {
        remove_char(FALSE);
    } else {
        for (rem = 0; rem < cfg_get_value_idx(CFG_TAB_WIDTH)->as_int && led.text[led.cur.cur] == ' '; ++rem)
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
    if (!led.is_undo) _append_action(backspace? ACTION_BACKSPACE : ACTION_DELETE, led.text[led.cur.cur]);
    memmove(led.text+led.cur.cur, led.text+led.cur.cur+1, led.text_sz-led.cur.cur);
    --led.text_sz;
    _count_lines();
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

static void _goto_start_of_selection(void) {
    selection_t sel = get_selection();
    if (led.cur.cur == sel.start) return;
    while (led.cur.cur > sel.start) move_left();
}

void remove_selection(void) {
    if (led.readonly) return;
    if (!is_selecting()) {
        led.cur.cur = led.lines[led.cur.line].start;
        led.cur.sel = led.lines[led.cur.line].end;
    }
    selection_t sel = get_selection();
    _goto_start_of_selection();
    // make sure it doesn't try to delete text out of bounds
    if (sel.end >= led.text_sz-1) sel.end = led.text_sz-2;
    if (!led.is_undo) {
        for (int i = sel.start; i <= sel.end; ++i)
            _append_action(ACTION_DELETE, led.text[i]);
    }
    memmove(led.text+sel.start, led.text+sel.end+1, led.text_sz-sel.end);
    led.text_sz -= (sel.end-sel.start)+1;
    _count_lines();
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
    // a bit roundabout, but probably still faster than pasting
    // from terminal and inserting the text character by character
    if (system("xsel -bo > /tmp/ledsel"));
    int sz;
    char *buf;
    FILE *file = fopen("/tmp/ledsel", "r");
    if (!file) return;
    fseek(file, 0, SEEK_END);
    buf = malloc((sz = ftell(file)) * sizeof(char));
    fseek(file, 0, SEEK_SET);
    if (fread(buf, sz, sizeof(char), file));
    fclose(file);
    insert_text(buf, sz);
    free(buf);
}

// XXX: to_lower and to_upper are wonky with undo/redo
void word_to_lower(void) {
    if (led.readonly) return;
    if (!is_selecting()) { move_next_word(); move_right(); }
    selection_t sel = get_selection();
    _goto_start_of_selection();
    for (led.cur.cur = sel.start; led.cur.cur < sel.end; move_right()) {
        char *c = &led.text[led.cur.cur];
        if (!islower(*c) && isalpha(*c)) {
            if (!led.is_undo) _append_action(ACTION_TOLOWER, *c);
            *c = tolower(*c);
        }
    }
    parse_tokens();
}

void word_to_upper(void) {
    if (led.readonly) return;
    if (!is_selecting()) { move_next_word(); move_right(); }
    selection_t sel = get_selection();
    _goto_start_of_selection();
    for (led.cur.cur = sel.start; led.cur.cur < sel.end; move_right()) {
        char *c = &led.text[led.cur.cur];
        if (!isupper(*c) && isalpha(*c)) {
            if (!led.is_undo) _append_action(ACTION_TOUPPER, *c);
            *c = toupper(*c);
        }
    }
    parse_tokens();
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

static int _last_token = 0;
static inline int _get_token_within(int pos) {
    // XXX: this is slow as fuck
    for (int i = _last_token; i < led.tokens_sz; ++i) {
        if (pos >= led.tokens[i].start && pos < led.tokens[i].end) {
            _last_token = i;
            return i;
        }
    }
    return -1;
}

static int _token_type_to_attr[] = {
    [LTK_IDENTIFIER] = ATTR_IDENTIFIER,
    [LTK_LITERAL]    = ATTR_LITERAL,
    [LTK_KEYWORD]    = ATTR_KEYWORD,
    [LTK_OPERATOR]   = ATTR_OPERATOR,
    [LTK_COMMENT]    = ATTR_COMMENT,
};

#if _USE_COLOR
static int _token_type_to_color[] = {
    [LTK_IDENTIFIER] = COLOR_PAIR(PAIR_NORMAL),
    [LTK_LITERAL]    = COLOR_PAIR(PAIR_LITERAL),
    [LTK_KEYWORD]    = COLOR_PAIR(PAIR_KEYWORD),
    [LTK_OPERATOR]   = COLOR_PAIR(PAIR_OPERATOR),
    [LTK_COMMENT]    = COLOR_PAIR(PAIR_COMMENT),
};
#endif

static void _render_line(int l, int off) {
    line_t line = led.lines[l];
    int sz = off;
    const int tab_width = cfg_get_value_idx(CFG_TAB_WIDTH)->as_int;
    token_t *tok = NULL;
    // XXX: this is slow as fuck
    for (int i = line.start; i <= line.end; ++i) {
        int attr = 0;
        const int tok_idx = _get_token_within(i);
        tok = (tok_idx == -1)? NULL : &led.tokens[tok_idx];
#if _USE_COLOR
        if (tok) attr = _token_type_to_attr[tok->type]|_token_type_to_color[tok->type];
#else
        if (tok) attr = _token_type_to_attr[tok->type];
#endif
        if (led.cur.cur == i || _is_selected(i)) attr = A_REVERSE | ((tok)? _token_type_to_attr[tok->type] : 0);
        attron(attr);
        mvprintw(l-led.cur.off, sz, "%c", isspace(led.text[i])? ' ':led.text[i]);
        if (led.text[i] == '\t') {
            if (_is_selected(i)) {
                for (int j = 0; j < tab_width; ++j)
                    mvprintw(l-led.cur.off, sz+j, " ");
            }
            sz += tab_width;
        } else sz++;
        attroff(attr);
    }
    if (cfg_get_value_idx(CFG_LINE_NUMBER)->as_bool)
        mvprintw(l-led.cur.off, 0, " %d ", l+1);
}

static inline int _calculate_line_size(void) {
    int sz = 0;
    for (int i = led.lines[led.cur.line].start; i < led.cur.cur; ++i)
        sz += (led.text[i] == '\t')? cfg_get_value_idx(CFG_TAB_WIDTH)->as_int : 1;
    return sz;
}

static void _render_text(void) {
    erase();
    _last_token = 0;
    int cur_off = _calculate_line_size(), off = 0;
    while (led.cur.line-led.cur.off < 0) { move_down(); led.cur.sel = led.cur.cur; }
    while (led.cur.line-led.cur.off >= led.wh-1) { move_up(); led.cur.sel = led.cur.cur; }
    if (cfg_get_value_idx(CFG_LINE_NUMBER)->as_bool) {
        char linenu[16];
        sprintf(linenu, " %ld ", led.lines_sz);
        off = strlen(linenu);
    }
    if (cur_off+off > led.ww-2) off = (led.ww-2)-cur_off;
    for (int i = led.cur.off; i < led.cur.off+led.wh-1; ++i) {
        if (i >= led.lines_sz) break;
        _render_line(i, off);
    }
}

static inline char *_mode_to_cstr(void) {
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

static void _render_status(void) {
    char status[ALLOC_SIZE] = {0};
    sprintf(status, " %s %d %d:%ld %s ",
        led.readonly? "[RO]" : "",
        led.cur.cur-led.lines[led.cur.line].start+1,
        led.cur.line+1, led.lines_sz, led.file);
    const size_t status_sz = strlen(status);
#if _USE_COLOR
    const int attr = ATTR_STATUS|COLOR_PAIR(COLOR_STATUS);
#else
    const int attr = ATTR_STATUS;
#endif
    attron(attr);
    mvprintw(led.wh-1, led.ww-status_sz, "%s", status);
    if (led.mode != MODE_NONE) {
        char *astr = _mode_to_cstr();
        mvprintw(led.wh-1, 0, "%s", astr);
        input_render(&led.input, strlen(astr), led.wh-1, led.ww-status_sz-strlen(astr));
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
    }
    if (ch == '\n' || ch == CTRL('r')) {
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
FUPDATE(_update_open, open_file(strndup(led.input.text, led.input.text_sz)))
FUPDATE(_update_command, cfg_parse(led.input.text, led.input.text_sz))

#undef FUPDATE

static void _update_insert(int ch) {
    switch (ch) {
#ifdef _USE_MTM // bruh
    case 200: {
        switch (getch()) {
        case 160: goto_line(1); break;
        case 155: goto_line(led.text_sz); break;
        case 170: move_prev_word(); break;
        case 171: return move_prev_word();
        case 185: move_next_word(); break;
        case 186: return move_next_word();
        case 144:
            if (is_selecting()) remove_selection();
            else remove_next_word();
            break;
        default: break;
        }
    } break;
    case 198: {
        switch (getch()) {
        case 137: return move_left();
        case 146: return move_right();
        case 135: return move_home();
        case 130: return move_end();
        case 142: return page_up();
        case 140: return page_down();
        default: break;
        }
    } break;
    case 197: {
        switch (getch()) {
        case 145: return move_up();
        case 144: return move_down();
        default: break;
        }
    } break;
#endif

    case CTRL('q'):
        exit_program();
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
        if (led.input.text_sz) _find_next(led.input);
        return;
    case CTRL('z'):
        undo_action(); break;
    case CTRL('y'):
        redo_action(); break;
    case CTRL('u'):
        word_to_upper(); break;
    case CTRL('l'):
        word_to_lower(); break;
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
        [MODE_FIND]    = &_update_find,
        [MODE_GOTO]    = &_update_goto,
        [MODE_OPEN]    = &_update_open,
        [MODE_REPLACE] = &_update_replace,
        [MODE_COMMAND] = &_update_command,
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
    _init_curses();
    for (;;) {
        getmaxyx(stdscr, led.wh, led.ww);
        _render_text();
        _render_status();
        _update();
    }
    _quit_curses();
    return 0;
}
