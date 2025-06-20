#ifndef __SYNTAX_NIM
#define __SYNTAX_NIM

#include <string.h>
#include <ctype.h>
#include "parser.h"

static const char *_NIM_EXTENSIONS[] = {".nim", NULL};
static const char *_NIM_KEYWORDS[] = {
    "var", "let", "const", "if", "elif", "else", "for", "while",
    "case", "return", "break", "continue", "discard", "defer",
    "proc", "import", "from", "include", "converter", "pragma",
    "static", "when", "try", "except", "finally", "type", "auto",
    "distinct", "template", "ref", "ptr", "typeof", "not", "nil",
    "local", "object", "iterator", "seq", "tuple", "array", "int",
    "float", "bool", "char", "string", "range", "uint", "and",
    "or", "in", "Natural", "float", "enum", "of", "cstring", "is",
    "assert", "method", "yield", "macro", "block", "do", "raise",
    "export", NULL,
};

#define LEX_INC(AMOUNT) { tok.end += AMOUNT; lex->cur += AMOUNT; }

static inline bool _nim_is_num(char c) {
    return strchr("xXoO_", c) != NULL || isxdigit(c);
}

static token_t _nim_next_token(syntax_t *syntax, lexer_t *lex) {
    token_t tok = {0};
    syntax->skip_space(syntax, lex);
    if (lex->cur >= lex->text_sz) return tok;

    tok.start = tok.end = lex->cur;
    if (lex->text[lex->cur] == '#') {
        tok.type = LTK_COMMENT;
        if (lex->cur+2 < lex->text_sz && lex->text[lex->cur+1] == '[') {
            while (lex->cur+2 < lex->text_sz && strncmp(lex->text+lex->cur, "]#", 2) != 0) LEX_INC(1);
            LEX_INC(1);
            return tok;
        }
        while (lex->text[lex->cur] != '\n' && lex->cur < lex->text_sz) LEX_INC(1);
        return tok;
    }
    if (lex->text[lex->cur] == '\"' || lex->text[lex->cur] == '\'' || lex->text[lex->cur] == '`') {
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
        while (_nim_is_num(lex->text[lex->cur]) && lex->cur < lex->text_sz) LEX_INC(1);
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

syntax_t syntax_nim(void) {
    return (syntax_t) {
        .name        = "nim",
        .extensions  = _NIM_EXTENSIONS,
        .keywords    = _NIM_KEYWORDS,
        .is_operator = default_is_operator,
        .is_word     = default_is_word,
        .next_token  = _nim_next_token,
        .skip_space  = default_skip_space,
    };
}

#endif
