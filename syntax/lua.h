#ifndef __SYNTAX_LUA
#define __SYNTAX_LUA

#include <string.h>
#include <ctype.h>
#include "parser.h"

static const char *_LUA_EXTENSIONS[] = {".lua", NULL};
static const char *_LUA_KEYWORDS[] = {
    "and", "break", "do", "else", "elseif", "end",
    "false", "for", "function", "if", "in", "local",
    "nil", "not", "or", "repeat", "return", "then",
    "true", "until", "while", NULL,
};

#define LEX_INC(AMOUNT) { tok.end += AMOUNT; lex->cur += AMOUNT; }

static inline bool _lua_is_num(char c) {
    return strchr("xXbBoO.", c) != NULL || isxdigit(c);
}

static token_t _lua_next_token(syntax_t *syntax, lexer_t *lex) {
    token_t tok = {0};
    syntax->skip_space(syntax, lex);
    if (lex->cur >= lex->text_sz) return tok;

    tok.start = tok.end = lex->cur;
    if (lex->cur+2 < lex->text_sz && strncmp(lex->text+lex->cur, "--", 2) == 0) {
        tok.type = LTK_COMMENT;
        while (lex->text[lex->cur] != '\n' && lex->cur < lex->text_sz) LEX_INC(1);
        return tok;
    }
    if (lex->text[lex->cur] == '\"' || lex->text[lex->cur] == '\'' || lex->text[lex->cur] == '`') {
        char match = lex->text[lex->cur];
        tok.type = LTK_LITERAL;
        do {
            int amnt = (lex->text[tok.end] == '\\')? 2 : 1;
            LEX_INC(1);
        } while (lex->text[lex->cur] != match && lex->cur < lex->text_sz);
        LEX_INC(1);
        return tok;
    }
    if (isdigit(lex->text[lex->cur])) {
        tok.type = LTK_LITERAL;
        while (_lua_is_num(lex->text[lex->cur]) && lex->cur < lex->text_sz) LEX_INC(1);
        return tok;
    }
    if (syntax->is_operator(syntax, lex->text[lex->cur])) {
        tok.type = LTK_OPERATOR;
        LEX_INC(1);
        return tok;
    }
    tok.type = LTK_IDENTIFIER;
    do LEX_INC(1)
    while (syntax->is_word(syntax, lex->text[lex->cur]) && lex->cur < lex->text_sz);
    if (default_is_keyword(syntax, lex, tok)) tok.type = LTK_KEYWORD;
    return tok;
}

#undef LEX_INC

syntax_t syntax_lua(void) {
    return (syntax_t) {
        .name        = "lua",
        .extensions  = _LUA_EXTENSIONS,
        .keywords    = _LUA_KEYWORDS,
        .is_operator = default_is_operator,
        .is_word     = default_is_word,
        .next_token  = _lua_next_token,
        .skip_space  = default_skip_space,
    };
}

#endif
