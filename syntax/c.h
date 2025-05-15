#ifndef __SYNTAX_C
#define __SYNTAX_C

#include "parser.h"

static const char *_C_EXTENSIONS[] = {".c", ".h", ".cc", ".cpp", ".hpp", NULL};
static const char *_C_KEYWORDS[] = {
    /* C Keywords */
    "auto", "break", "case", "continue", "default", "do", "else", "enum",
    "extern", "for", "goto", "if", "register", "return", "sizeof", "static",
    "struct", "switch", "typedef", "union", "volatile", "while",
    "NULL", "TRUE", "FALSE",

    /* C++ Keywords */
    "alignas", "alignof", "and", "and_eq", "asm", "bitand", "bitor", "class",
    "compl", "constexpr", "const_cast", "deltype", "delete", "dynamic_cast",
    "explicit", "export", "false", "friend", "inline", "mutable", "namespace",
    "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq",
    "private", "protected", "public", "reinterpret_cast", "static_assert",
    "static_cast", "template", "this", "thread_local", "throw", "true", "try",
    "typeid", "typename", "virtual", "xor", "xor_eq",

    /* C types */
    "int", "long", "double", "float", "char", "unsigned", "signed",
    "void", "short", "auto", "const", "bool", NULL
};

syntax_t syntax_c(void) {
    return (syntax_t) {
        .name        = "c",
        .extensions  = _C_EXTENSIONS,
        .keywords    = _C_KEYWORDS,
        .is_operator = default_is_operator,
        .is_word     = default_is_word,
        .next_token  = default_next_token,
        .skip_space  = default_skip_space,
    };
}
#endif
