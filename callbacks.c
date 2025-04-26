#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "callbacks.h"
#include "config.h"
#include "led.h"

static inline char* parse_string(void);

void command_set(void) {
    char *key = parse_string();
    if (!key) return;
    cfg_token_t tok_val = cfg_next_token();
    cfg_value_t *val = cfg_get_value_key(key);
    free(key);
    if (val) {
        if (val->type == CTYP_STR) free(val->as_str);
        *val = cfg_parse_value(tok_val);
    }
}

void command_load(void) {
    char *file = parse_string();
    if (!file) return;
    cfg_parse_file(file);
    free(file);
}

void command_open(void) {
    char *file = parse_string();
    if (!file) return;
    open_file(file);
}

void command_save(void) {
    char *file = parse_string();
    if (!file) return;
    write_file(file);
    free(file);
}

void command_quit(void) {
    exit_program();
}

static inline char* parse_string(void) {
    cfg_token_t tok = cfg_next_token();
    if (!tok.sz) return NULL;
    cfg_value_t val = cfg_parse_value(tok);
    if (val.type == CTYP_STR) return val.as_str;
    return strndup(tok.ptr, tok.sz);
}

