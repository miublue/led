#ifndef __SYNTAX_RUST
#define __SYNTAX_RUST

#include "parser.h"

static const char *_RUST_EXTENSIONS[] = {".rs", NULL};
static const char *_RUST_KEYWORDS[] = {
    "as", "use", "extern", "break", "const",
    "continue", "crate", "else", "if", "final",
    "enum", "extern", "false", "fn", "for",
    "impl", "in", "for", "let", "loop", "match",
    "mod", "move", "mut", "pub", "impl", "ref",
    "return", "Self", "self", "static", "struct",
    "super", "trait", "true", "type", "unsafe",
    "use", "where", "while", "abstract", "alignof",
    "become", "box", "do", "final", "macro", "offsetof",
    "override", "priv", "proc", "pure", "sizeof",
    "typeof", "unsized", "virtual", "yield", "try",
    "dyn", "async", "await", "union", "macro_rules",

    "i8", "i16", "i32", "i64", "i128", "isize",
    "u8", "u16", "u32", "u64", "u128", "usize",
    "f32", "f64", "f128", "bool", "char", "str",
    "String", "Vec",
    NULL
};

#define LEX_INC(AMOUNT) { tok.end += AMOUNT; lex->cur += AMOUNT; }

static inline bool _rust_is_num(char c) {
    return strchr("xXoO", c) != NULL || isxdigit(c);
}

static token_t _rust_next_token(syntax_t *syntax, lexer_t *lex) {
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

    if (lex->text[lex->cur] == '\"' || lex->text[lex->cur] == '`') {
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
        while (_rust_is_num(lex->text[lex->cur]) && lex->cur < lex->text_sz) LEX_INC(1);
        return tok;
    }
    if (syntax->is_operator(syntax, lex->text[lex->cur])) {
        tok.type = LTK_OPERATOR;
        LEX_INC(1);
        return tok;
    }
    tok.type = LTK_IDENTIFIER;
    do LEX_INC(1) while (syntax->is_word(syntax, lex->text[lex->cur]) && lex->cur < lex->text_sz);
    if (default_is_keyword(syntax, lex, tok)) tok.type = LTK_KEYWORD;
    return tok;
}

#undef LEX_INC

syntax_t syntax_rust(void) {
    return (syntax_t) {
        .name        = "rust",
        .extensions  = _RUST_EXTENSIONS,
        .keywords    = _RUST_KEYWORDS,
        .is_operator = default_is_operator,
        .is_word     = default_is_word,
        .next_token  = _rust_next_token,
        .skip_space  = default_skip_space,
    };
}
#endif
