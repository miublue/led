#ifndef __SYNTAX_BCPL
#define __SYNTAX_BCPL

#include <string.h>
#include <ctype.h>
#include "parser.h"

static const char *_BCPL_EXTENSIONS[] = {".b", ".bcpl", NULL};
static const char *_BCPL_KEYWORDS[] = {
    "and", "abs", "be", "break", "by", "case", "do", "default", "eq",
    "eqv", "else", "endcase", "false", "for", "finish", "goto", "ge",
    "get", "gr", "global", "gt", "if", "into", "let", "lv", "le", "ls",
    "logor", "logand", "loop", "lshift", "manifest", "ne", "not",
    "neqv", "needs", "or", "resultis", "return", "rem", "rshift",
    "rv", "repeat", "repeatwhile", "repeatuntil", "switchon", "static",
    "section", "to", "test", "true", "then", "table", "until", "unless",
    "vec", "valof", "while", NULL,
};

#define LEX_INC(AMOUNT) { tok.end += AMOUNT; lex->cur += AMOUNT; }

static bool _bcpl_is_num(char c) {
    return strchr("xX", c) != NULL || isxdigit(c);
}

static bool _bcpl_is_operator(syntax_t *syntax, char c) {
    return strchr("@+-*/%!=<>,:;?&$|(){}[]~^\\", c) != NULL;
}

static inline bool _bcpl_is_keyword(syntax_t *syntax, lexer_t *lex, token_t tok) {
    const int sz = tok.end-tok.start;
    const char *ptr = lex->text+tok.start;
    for (int i = 0; syntax->keywords[i]; ++i) {
        if (strlen(syntax->keywords[i]) == sz && !strncasecmp(syntax->keywords[i], ptr, sz))
            return true;
    }
    return false;
}

static token_t _bcpl_next_token(syntax_t *syntax, lexer_t *lex) {
    token_t tok = {0};
    syntax->skip_space(syntax, lex);
    if (lex->cur >= lex->text_sz) return tok;

    tok.start = tok.end = lex->cur;
    if (lex->cur+1 < lex->text_sz && lex->text[lex->cur] == '/') {
        if (lex->text[lex->cur+1] == '/') {
            tok.type = LTK_COMMENT;
            while (lex->cur < lex->text_sz && lex->text[lex->cur] != '\n') LEX_INC(1);
            return tok;
        }
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
    if (isdigit(lex->text[lex->cur]) || lex->text[lex->cur] == '#') {
        tok.type = LTK_LITERAL;
        do {
            LEX_INC(1);
        } while (_bcpl_is_num(lex->text[lex->cur]) && lex->cur < lex->text_sz);
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
    if (_bcpl_is_keyword(syntax, lex, tok)) tok.type = LTK_KEYWORD;
    return tok;
}

#undef LEX_INC

syntax_t syntax_bcpl(void) {
    return (syntax_t) {
        .name        = "bcpl",
        .extensions  = _BCPL_EXTENSIONS,
        .keywords    = _BCPL_KEYWORDS,
        .is_operator = _bcpl_is_operator,
        .is_word     = default_is_word,
        .next_token  = _bcpl_next_token,
        .skip_space  = default_skip_space,
    };
}

#endif
