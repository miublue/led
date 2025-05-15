#ifndef __CONFIG_H
#define __CONFIG_H

#include <stdbool.h>

enum {
    CFG_TAB_WIDTH,
    CFG_EXPAND_TAB,
    CFG_LINE_NUMBER,
    CFG_HIGHLIGHT,
    NUM_CONFIG_VALUES,
};

typedef void (*cfg_callback_t)();
typedef struct { char *ptr; int sz; } cfg_token_t;

typedef struct {
    enum { CTYP_BOOL, CTYP_INT, CTYP_STR } type;
    union {
        bool as_bool; // lul
        long as_int;
        char *as_str;
    };
} cfg_value_t;

#define CVAL_BOOL(BOOL) ((cfg_value_t){.type = CTYP_BOOL, .as_bool = BOOL})
#define CVAL_INT(NUM) ((cfg_value_t){.type = CTYP_INT, .as_int = NUM})
#define CVAL_STR(STR) ((cfg_value_t){.type = CTYP_STR, .as_str = STR})

cfg_value_t *cfg_get_value_idx(int id);
cfg_value_t *cfg_get_value_key(char *key);
cfg_value_t cfg_parse_value(cfg_token_t tok);
bool cfg_compare_tok(cfg_token_t tok, const char *str);
cfg_token_t cfg_next_token(void);
void cfg_parse(char *text, int sz);
void cfg_parse_file(char *path);

#endif
