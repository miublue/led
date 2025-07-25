#ifndef __SYNTAX_B
#define __SYNTAX_B

#include <string.h>
#include <ctype.h>
#include "parser.h"

static const char *_B_EXTENSIONS[] = {".b", NULL};
static const char *_B_KEYWORDS[] = {
    "auto", "extrn", "case", "if", "else", "while",
    "switch", "goto", "return", NULL,
};

#define LEX_INC(AMOUNT) { tok.end += AMOUNT; lex->cur += AMOUNT; }

static bool _b_is_num(char c) {
    return strchr("xX", c) != NULL || isxdigit(c);
}

static bool _b_is_operator(syntax_t *syntax, char c) {
    return strchr("+-*/%!=<>,:;?&|(){}[]~^", c) != NULL;
}

static token_t _b_next_token(syntax_t *syntax, lexer_t *lex) {
    token_t tok = {0};
    syntax->skip_space(syntax, lex);
    if (lex->cur >= lex->text_sz) return tok;

    tok.start = tok.end = lex->cur;
    if (lex->cur+1 < lex->text_sz && lex->text[lex->cur] == '/') {
        if (lex->text[lex->cur+1] == '*') {
            tok.type = LTK_COMMENT;
            while (lex->cur < lex->text_sz) {
                if (lex->cur+1 < lex->text_sz && !strncmp(lex->text+lex->cur, "*/", 2)) break;
                LEX_INC(1);
            }
            LEX_INC(2);
            return tok;
        }
    }

    if (lex->text[lex->cur] == '\"' || lex->text[lex->cur] == '\'') {
        char match = lex->text[lex->cur];
        tok.type = LTK_LITERAL;
        do {
            int amnt = (lex->text[tok.end] == '*')? 2 : 1;
            LEX_INC(amnt);
        } while (lex->text[lex->cur] != match && lex->cur < lex->text_sz);
        LEX_INC(1);
        return tok;
    }
    if (isdigit(lex->text[lex->cur])) {
        tok.type = LTK_LITERAL;
        while (_b_is_num(lex->text[lex->cur]) && lex->cur < lex->text_sz) LEX_INC(1);
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

syntax_t syntax_b(void) {
    return (syntax_t) {
        .name        = "b",
        .extensions  = _B_EXTENSIONS,
        .keywords    = _B_KEYWORDS,
        .is_operator = _b_is_operator,
        .is_word     = default_is_word,
        .next_token  = _b_next_token,
        .skip_space  = default_skip_space,
    };
}

#endif
