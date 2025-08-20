#ifndef __SYNTAX_N
#define __SYNTAX_N

#include <string.h>
#include <ctype.h>
#include "parser.h"

static const char *_N_EXTENSIONS[] = {".n", NULL};
static const char *_N_KEYWORDS[] = {
    "if", "else", "while", "break", "fun",
    "return", "extern", "inline", "struct",
    "char", "int", "ptr", "null",
    NULL,
};

#define LEX_INC(AMOUNT) { tok.end += AMOUNT; lex->cur += AMOUNT; }

static inline bool _n_is_num(char c) {
    return strchr("xX._", c) != NULL || isxdigit(c);
}

static inline bool _n_is_operator(syntax_t *syntax, char c) {
    return strchr("+-*/%!@=<>.,:;&|(){}[]~^#", c) != NULL;
}

static token_t _n_next_token(syntax_t *syntax, lexer_t *lex) {
    token_t tok = {0};
    syntax->skip_space(syntax, lex);
    if (lex->cur >= lex->text_sz) return tok;

    tok.start = tok.end = lex->cur;
    if (lex->text[lex->cur] == '#') {
        tok.type = LTK_COMMENT;
        if (lex->cur+2 < lex->text_sz && lex->text[lex->cur+1] == '[') {
            while (lex->cur+2 < lex->text_sz && strncmp(lex->text+lex->cur, "]#", 2) != 0) LEX_INC(1);
            LEX_INC(2);
            return tok;
        }
        while (lex->text[lex->cur] != '\n' && lex->cur < lex->text_sz) LEX_INC(1);
        return tok;
    }
    if (lex->text[lex->cur] == '\"' || lex->text[lex->cur] == '\'') {
        char match = lex->text[lex->cur];
        tok.type = LTK_LITERAL;
        do {
            int amnt = (lex->text[tok.end] == '\\')? 2 : 1;
            LEX_INC(amnt);
        } while (lex->text[lex->cur] != match && lex->cur < lex->text_sz);
        LEX_INC(1);
        return tok;
    }
    if (isdigit(lex->text[lex->cur])) {
        tok.type = LTK_LITERAL;
        while (_n_is_num(lex->text[lex->cur]) && lex->cur < lex->text_sz) LEX_INC(1);
        return tok;
    }
    if (syntax->is_operator(syntax, lex->text[lex->cur])) {
        tok.type = LTK_OPERATOR;
        LEX_INC(1);
        return tok;
    }
    tok.type = LTK_IDENTIFIER;
    do {
        LEX_INC(1);
    } while (syntax->is_word(syntax, lex->text[lex->cur]) && lex->cur < lex->text_sz);
    if (default_is_keyword(syntax, lex, tok)) tok.type = LTK_KEYWORD;
    return tok;
}

#undef LEX_INC

syntax_t syntax_n(void) {
    return (syntax_t) {
        .name        = "n",
        .extensions  = _N_EXTENSIONS,
        .keywords    = _N_KEYWORDS,
        .is_operator = _n_is_operator,
        .is_word     = default_is_word,
        .next_token  = _n_next_token,
        .skip_space  = default_skip_space,
    };
}

#endif
