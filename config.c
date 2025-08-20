#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "config.h"
#include "callbacks.h"
#include "led.h"

static struct { char *name; cfg_callback_t callback; } _config_commands[] = {
    { "set",    command_set },
    { "open",   command_open },
    { "load",   command_load },
    { "save",   command_save },
    { "quit",   command_quit },
    { "syntax", command_syntax },
};

static struct { char *key; cfg_value_t val; } _config_values[NUM_CONFIG_VALUES] = {
    [CFG_TAB_WIDTH]    = { "tabwidth",   CVAL_INT(4) },
    [CFG_EXPAND_TAB]   = { "expandtab",  CVAL_BOOL(true) },
    [CFG_LINE_NUMBER]  = { "linenumber", CVAL_BOOL(false) },
    [CFG_HIGHLIGHT]    = { "highlight",  CVAL_BOOL(true) },
    [CFG_IGNORE_CASE]  = { "ignorecase", CVAL_BOOL(false) },
};

static struct { char *text; int sz, cur; } cfg;

static void skip_space(void) {
    while (cfg.cur < cfg.sz && isspace(cfg.text[cfg.cur])) ++cfg.cur;
    if (cfg.cur < cfg.sz && cfg.text[cfg.cur] == '#') {
        while (cfg.cur < cfg.sz && cfg.text[cfg.cur] != '\n') ++cfg.cur;
        skip_space();
    }
}

cfg_token_t cfg_next_token(void) {
    cfg_token_t tok = {.ptr = NULL, .sz = 0};
    skip_space();
    if (cfg.cur >= cfg.sz) return tok;
    tok.ptr = cfg.text+cfg.cur;
    if (cfg.text[cfg.cur] == '\"') {
        do {
            ++tok.sz;
            ++cfg.cur;
        } while (cfg.cur < cfg.sz && cfg.text[cfg.cur] != '\"');
        ++tok.sz;
        ++cfg.cur;
        return tok;
    }
    while (cfg.cur < cfg.sz && !isspace(cfg.text[cfg.cur])) {
        ++tok.sz;
        ++cfg.cur;
    }
    return tok;
}

static long parse_int(cfg_token_t tok) {
    if (!tok.ptr || !tok.sz) return 0;
    char *end = tok.ptr+tok.sz;
    return strtol(tok.ptr, &end, 0);
}

bool cfg_compare_tok(cfg_token_t tok, const char *str) {
    return strlen(str) == tok.sz && strncmp(tok.ptr, str, tok.sz) == 0;
}

cfg_value_t cfg_parse_value(cfg_token_t tok) {
    cfg_value_t val = CVAL_INT(0);
    bool is_true = cfg_compare_tok(tok, "true") || cfg_compare_tok(tok, "yes");
    bool is_false = cfg_compare_tok(tok, "false") || cfg_compare_tok(tok, "no");
    if (isdigit(tok.ptr[0]) || tok.ptr[0] == '-')
        val = CVAL_INT(parse_int(tok));
    else if (tok.ptr[0] == '\"')
        val = CVAL_STR(strndup(tok.ptr+1, tok.sz-2));
    else if (is_true || is_false)
        val = CVAL_BOOL(is_true);
    return val;
}

cfg_value_t *cfg_get_value_idx(int id) {
    return (id >= NUM_CONFIG_VALUES)? NULL : &_config_values[id].val;
}

cfg_value_t *cfg_get_value_key(char *key) {
    for (int i = 0; i < LENGTH(_config_values); ++i) {
        if (!strcmp(_config_values[i].key, key))
            return &_config_values[i].val;
    }
    return NULL;
}

void cfg_parse(char *text, int sz) {
    cfg.text = text;
    cfg.sz = sz;
    cfg.cur = 0;
    while (cfg.cur < cfg.sz) {
        cfg_token_t tok = cfg_next_token();
        for (int i = 0; i < LENGTH(_config_commands); ++i) {
            if (cfg_compare_tok(tok, _config_commands[i].name)) {
                _config_commands[i].callback();
                parse_tokens();
                break;
            }
        }
    }
}

void cfg_parse_file(char *path) {
    FILE *file = NULL;
    char *text = NULL;
    int sz = 0;
    char *cfg_text = cfg.text;
    int cfg_sz = cfg.sz, cfg_cur = cfg.cur;
    if (!(file = fopen(path, "r"))) goto fail;
    fseek(file, 0, SEEK_END);
    sz = ftell(file);
    rewind(file);
    if (!(text = malloc(sz))) goto fail;
    if (fread(text, 1, sz, file) != sz) goto fail;
    cfg_parse(text, sz);
    cfg.text = cfg_text;
    cfg.sz = cfg_sz;
    cfg.cur = cfg_cur;
    free(text);
    fclose(file);
    return;
fail:
    if (file) fclose(file);
    if (text) free(text);
}
