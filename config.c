#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "config.h"

#define LENGTH(x) (sizeof(x)/sizeof((x)[0]))

// default values, they should be changeable through a config file later
static struct { char *key; ConfigValue val; } config_values[NUM_CONFIG_VALUES] = {
    [TAB_WIDTH]   = { "tab_width", CVAL_INT(4) },
    [EXPAND_TAB]  = { "expand_tab", CVAL_BOOL(true) },
};

ConfigValue *get_config_value(int id) {
    return (id >= NUM_CONFIG_VALUES)? NULL : &config_values[id].val;
}

ConfigValue *get_config_value_str(char *key) {
    for (int i = 0; i < LENGTH(config_values); ++i) {
        if (!strcmp(config_values[i].key, key))
            return &config_values[i].val;
    }
    return NULL;
}

static struct { char *text; int sz, cur; } cfg;
struct cfg_token { char *ptr; int sz; };

static void skip_space(void) {
    while (cfg.cur < cfg.sz && isspace(cfg.text[cfg.cur])) ++cfg.cur;
    if (cfg.cur < cfg.sz && cfg.text[cfg.cur] == '#') {
        while (cfg.cur < cfg.sz && cfg.text[cfg.cur] != '\n') ++cfg.cur;
        skip_space();
    }
}

static struct cfg_token next_token(void) {
    struct cfg_token tok = {.ptr = NULL, .sz = 0};
    skip_space();
    if (cfg.cur >= cfg.sz) return tok;
    tok.ptr = cfg.text+cfg.cur;
    while (cfg.cur < cfg.sz && !isspace(cfg.text[cfg.cur])) {
        ++tok.sz;
        ++cfg.cur;
    }
    return tok;
}

static long parse_int(struct cfg_token tok) {
    if (!tok.ptr || !tok.sz) return 0;
    char *end = tok.ptr+tok.sz;
    return strtol(tok.ptr, &end, 0);
}

static int compare_tok(struct cfg_token tok, const char *str) {
    return strlen(str) == tok.sz && strncmp(tok.ptr, str, tok.sz) == 0;
}

static void change_config(struct cfg_token tok) {
    ConfigValue cfg_val = {0};
    struct cfg_token tok_val = {0};
    if (cfg.cur < cfg.sz) tok_val = next_token();
    bool is_true = compare_tok(tok_val, "true") || compare_tok(tok_val, "yes");
    bool is_false = compare_tok(tok_val, "false") || compare_tok(tok_val, "no");

    if (!tok_val.sz) {
        cfg_val = CVAL_INT(0);
    } else if (isdigit(tok_val.ptr[0])) {
        cfg_val = CVAL_INT(parse_int(tok_val));
    } else if (is_true || is_false) {
        cfg_val = CVAL_BOOL(is_true);
    } else {
        cfg_val = CVAL_INT(0);
        // XXX: add strings when i need them
    }

    char *key = strndup(tok.ptr, tok.sz);
    ConfigValue *val = get_config_value_str(key);
    if (val) *val = cfg_val;
    free(key);
}

void parse_config(char *text, int sz) {
    cfg.text = text;
    cfg.sz = sz;
    cfg.cur = 0;
    while (cfg.cur < cfg.sz) {
        struct cfg_token tok = next_token();
        if (compare_tok(tok, "set")) change_config(next_token());
    }
}

