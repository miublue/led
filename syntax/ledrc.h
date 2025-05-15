#ifndef __SYNTAX_LEDRC
#define __SYNTAX_LEDRC

#include <string.h>
#include <ctype.h>
#include "parser.h"

static const char *_LEDRC_EXTENSIONS[] = {".ledrc", "ledrc", NULL};
static const char *_LEDRC_KEYWORDS[] = {
    "load", "open", "save", "set", "syntax",
    "true", "false", "yes", "no", NULL,
};

#define LEX_INC(AMOUNT) { tok.end += AMOUNT; lex->cur += AMOUNT; }

static token_t _ledrc_next_token(syntax_t *syntax, lexer_t *lex) {
    token_t tok = {0};
    syntax->skip_space(syntax, lex);
    if (lex->cur >= lex->text_sz) return tok;

    tok.start = tok.end = lex->cur;
    if (lex->text[lex->cur] == '#') {
        tok.type = LTK_COMMENT;
        while (lex->text[lex->cur] != '\n' && lex->cur < lex->text_sz) LEX_INC(1);
        return tok;
    }
    if (lex->text[lex->cur] == '\"') {
        tok.type = LTK_LITERAL;
        do {
            int amnt = (lex->text[tok.end] == '\\')? 2 : 1;
            LEX_INC(amnt);
        } while (lex->text[lex->cur] != '\"' && lex->cur < lex->text_sz);
        LEX_INC(1);
        return tok;
    }
    if (isdigit(lex->text[lex->cur])) {
        tok.type = LTK_LITERAL;
        while (isdigit(lex->text[lex->cur]) && lex->cur < lex->text_sz) LEX_INC(1);
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

syntax_t syntax_ledrc(void) {
    return (syntax_t) {
        .name        = "ledrc",
        .extensions  = _LEDRC_EXTENSIONS,
        .keywords    = _LEDRC_KEYWORDS,
        .is_operator = default_is_operator,
        .is_word     = default_is_word,
        .next_token  = _ledrc_next_token,
        .skip_space  = default_skip_space,
    };
}

#endif
