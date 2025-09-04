#ifndef __SYNTAX_ZIG
#define __SYNTAX_ZIG

#include "parser.h"

static const char *_ZIG_EXTENSIONS[] = {".zig", NULL};
static const char *_ZIG_KEYWORDS[] = {
    "addrspace", "align", "allowzero", "and",
    "anyframe", "anytype", "asm", "async", "await", "break",
    "callconv", "catch", "comptime", "const", "continue",
    "defer", "else", "enum", "errdefer", "error",
    "export", "extern", "fn", "for", "if", "inline",
    "linksection", "noalias", "noinline", "nosuspend",
    "opaque", "or", "orelse", "packed", "pub", "resume",
    "return", "struct", "suspend", "switch", "test",
    "threadLocal", "try", "union", "unreachable",
    "usingnamespace", "var", "volatile", "while",
    "void", "bool", "isize", "usize", "c_char", "c_short",
    "c_ushort", "c_int", "c_uint", "c_long", "c_ulong",
    "c_longlong", "c_ulonglong", "c_longdouble", "anyopaque",
    "noreturn", "type", "anyerror", "comptime_int", "comptime_float",
    "true", "false", "null", "undefined",
    NULL
};

#define LEX_INC(AMOUNT) { tok.end += AMOUNT; lex->cur += AMOUNT; }

static inline bool _zig_is_operator(syntax_t *syntax, char c) {
    return strchr("+-*/%^~!=<>?:;.,&|(){}[]$", c) != NULL;
}

static inline bool _zig_is_num(char c) {
    return strchr("xXbBoO._", c) != NULL || isxdigit(c);
}

static token_t _zig_next_token(syntax_t *syntax, lexer_t *lex) {
    token_t tok = {0};
    syntax->skip_space(syntax, lex);
    if (lex->cur >= lex->text_sz) return tok;

    tok.start = tok.end = lex->cur;
    if (lex->cur+1 < lex->text_sz && lex->text[lex->cur] == '/') {
        if (lex->text[lex->cur+1] == '/') {
            tok.type = LTK_COMMENT;
            while (lex->text[lex->cur] != '\n' && lex->cur < lex->text_sz) LEX_INC(1);
            return tok;
        }
        else if (lex->text[lex->cur+1] == '*') {
            tok.type = LTK_COMMENT;
            while (lex->cur < lex->text_sz) {
                if (lex->cur+1 < lex->text_sz && !strncmp(lex->text+lex->cur, "*/", 2)) break;
                LEX_INC(1);
            }
            LEX_INC(2);
            return tok;
        }
    }

    if (lex->text[lex->cur] == '\"' || lex->text[lex->cur] == '\'' || lex->text[lex->cur] == '`') {
        char match = lex->text[lex->cur];
        tok.type = LTK_LITERAL;
        do {
            const int amnt = (lex->text[tok.end] == '\\')? 2 : 1;
            LEX_INC(amnt);
        } while (lex->text[lex->cur] != match && lex->cur < lex->text_sz);
        LEX_INC(1);
        return tok;
    }
    if (isdigit(lex->text[lex->cur])) {
        tok.type = LTK_LITERAL;
        while (_zig_is_num(lex->text[lex->cur]) && lex->cur < lex->text_sz) LEX_INC(1);
        return tok;
    }
    if (syntax->is_operator(syntax, lex->text[lex->cur])) {
        tok.type = LTK_OPERATOR;
        LEX_INC(1);
        return tok;
    }
    tok.type = LTK_IDENTIFIER;
    do LEX_INC(1) while (syntax->is_word(syntax, lex->text[lex->cur]) && lex->cur < lex->text_sz);
    if (default_is_keyword(syntax, lex, tok) || lex->text[tok.start] == '@') tok.type = LTK_KEYWORD;
    return tok;
}

#undef LEX_INC

syntax_t syntax_zig(void) {
    return (syntax_t) {
        .name        = "zig",
        .extensions  = _ZIG_EXTENSIONS,
        .keywords    = _ZIG_KEYWORDS,
        .is_operator = _zig_is_operator,
        .is_word     = default_is_word,
        .next_token  = _zig_next_token,
        .skip_space  = default_skip_space,
    };
}

#endif
