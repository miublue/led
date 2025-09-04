#ifndef __SYNTAX_D
#define __SYNTAX_D

#include "parser.h"

static const char *_D_EXTENSIONS[] = {".d", ".di", NULL};
static const char *_D_KEYWORDS[] = {
    "abstract", "alias", "align", "asm", "assert",
    "auto", "body", "bool", "break", "byte", "case",
    "cast", "catch", "cdouble", "cent", "cfloat",
    "char", "class", "const", "continue", "creal",
    "dchar", "debug", "default", "delegate", "delete",
    "deprecated", "do", "double", "else", "enum", "export",
    "extern", "false", "final", "finally", "float", "for",
    "foreach", "foreach_reverse", "function", "goto",
    "idouble", "if", "ifloat", "immutable", "import", "in",
    "inout", "int", "interface", "invariant", "ireal", "is",
    "lazy", "long", "macro", "mixin", "module", "new",
    "nothrow", "null", "out", "override", "package", "pragma",
    "private", "protected", "public", "pure", "real", "ref",
    "return", "scope", "shared", "short", "static", "struct",
    "super", "switch", "synchronized", "template", "this",
    "throw", "true", "try", "while", "with", "typeid",
    "typeof", "ubyte", "ucent", "uint", "ulong", "union",
    "unittest", "ushort", "version", "void", "wchar",
    "string", "dstring", "wstring",
    "__FILE__", "__FILE_FULL_PATH__", "__MODULE__",
    "__LINE__", "__FUNCTION__", "__PRETTY_FUNCTION__",
    "__gshared", "__traits", "__vector", "__parameters",
    NULL
};

syntax_t syntax_d(void) {
    return (syntax_t) {
        .name        = "d",
        .extensions  = _D_EXTENSIONS,
        .keywords    = _D_KEYWORDS,
        .is_operator = default_is_operator,
        .is_word     = default_is_word,
        .next_token  = default_next_token,
        .skip_space  = default_skip_space,
    };
}

#endif
