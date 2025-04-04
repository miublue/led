#ifndef __CONFIG_H
#define __CONFIG_H

#include <stdbool.h>

enum {
    TAB_WIDTH,
    EXPAND_TAB,
    NUM_CONFIG_VALUES,
};

typedef struct config_value_t {
    enum { CTYP_BOOL, CTYP_INT, CTYP_STR } type;
    union {
        bool as_bool; // lul
        long as_int;
        char *as_str;
    };
} ConfigValue;

#define CVAL_BOOL(BOOL) ((ConfigValue){.type = CTYP_BOOL, .as_bool = BOOL})
#define CVAL_INT(NUM) ((ConfigValue){.type = CTYP_INT, .as_int = NUM})
#define CVAL_STR(STR) ((ConfigValue){.type = CTYP_STR, .as_str = STR})

ConfigValue *get_config_value(int id);
ConfigValue *get_config_value_str(char *key);
void parse_config(char *text, int sz);

#endif
