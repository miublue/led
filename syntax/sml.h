#ifndef __SYNTAX_SML
#define __SYNTAX_SML

#include <string.h>
#include <ctype.h>
#include "parser.h"

static const char *_SML_EXTENSIONS[] = {".sml", ".sig", NULL};
// XXX: i can't find a single list of sml keywords, so i'll be adding them as i need them
static const char *_SML_KEYWORDS[] = {
    "fun", "fn", "val", "let", "in", "as", "end",
    "if", "then", "else", "while", "local", "and",
    "or", "andalso", "orelse", "type", "datatype",
    "exception", "raise", "handle", "functor", "of",
    "struct", "structure", "signature", "sig", "not",
    "case", "ref", "open", "infixr", "infixl", "infix",
    "div", "mod",

    "false", "true", "bool", "int", "float", "list",
    "array", "char", "string", "option", "SOME", "NONE",

NULL,
};

#define LEX_INC(AMOUNT) { tok.end += AMOUNT; lex->cur += AMOUNT; }

static inline bool _sml_is_num(char c) {
    return strchr("xXbBoO.", c) != NULL || isxdigit(c);
}

static token_t _sml_next_token(syntax_t *syntax, lexer_t *lex) {
    token_t tok = {0};
    syntax->skip_space(syntax, lex);
    if (lex->cur >= lex->text_sz) return tok;

    tok.start = tok.end = lex->cur;
    if (lex->cur+2 < lex->text_sz && strncmp(lex->text+lex->cur, "(*", 2) == 0) {
        tok.type = LTK_COMMENT;
        while (lex->cur+2 < lex->text_sz && strncmp(lex->text+lex->cur, "*)", 2) != 0) LEX_INC(1);
        LEX_INC(2);
        return tok;
    }
    if (strchr("\"`", lex->text[lex->cur])) {
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
        while (_sml_is_num(lex->text[lex->cur]) && lex->cur < lex->text_sz) LEX_INC(1);
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

syntax_t syntax_sml(void) {
    return (syntax_t) {
        .name        = "sml",
        .extensions  = _SML_EXTENSIONS,
        .keywords    = _SML_KEYWORDS,
        .is_operator = default_is_operator,
        .is_word     = default_is_word,
        .next_token  = _sml_next_token,
        .skip_space  = default_skip_space,
    };
}

#endif
