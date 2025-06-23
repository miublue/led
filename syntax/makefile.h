#ifndef __SYNTAX_MAKEFILE
#define __SYNTAX_MAKEFILE

#include <string.h>
#include <ctype.h>
#include "parser.h"

static const char *_MAKEFILE_EXTENSIONS[] = {"makefile", "Makefile", "MAKEFILE", ".mk", ".make", ".mkfile", NULL};
static const char *_MAKEFILE_KEYWORDS[] = {
    "define", "undefine", "endef", "ifeq", "ifdef", "ifneq", "ifndef", "endif", "else",
    "include", "sinclude", "override", "export", "unexport", "private", "vpath", NULL
};

#define LEX_INC(AMOUNT) { tok.end += AMOUNT; lex->cur += AMOUNT; }
#define LEX_INCP(AMOUNT) { tok->end += AMOUNT; lex->cur += AMOUNT; }

static bool _makefile_is_keyword(syntax_t *syntax, lexer_t *lex, token_t tok) {
    if (default_is_keyword(syntax, lex, tok)) return true;
    if (lex->text[tok.end-1] == ':') return true;
    return false;
}

static bool _makefile_is_operator(syntax_t *syntax, char c) {
    return strchr("(){}[]$?=+-@!&,:", c) != NULL;
}

static inline bool _makefile_is_bracket(char c) {
    return strchr("({[", c) != NULL;
}

static inline bool _makefile_match_bracket(char bracket, char c) {
    if (bracket == '(') return c == ')';
    if (bracket == '{') return c == '}';
    if (bracket == '[') return c == ']';
    return false;
}

static inline bool _makefile_is_quote(char c) {
    return strchr("\"\'`", c) != NULL;
}

static inline void _makefile_skip_quote(syntax_t *syntax, lexer_t *lex, token_t *tok) {
    if (!_makefile_is_quote(lex->text[lex->cur])) return;
    char quote = lex->text[lex->cur];
    if (lex->cur > 0 && lex->text[lex->cur-1] == '\\') return;
    do {
        int amnt = (lex->text[tok->end] == '\\')? 2 : 1;
        LEX_INCP(amnt);
    } while (lex->cur < lex->text_sz && lex->text[lex->cur] != quote);
}

static inline void _makefile_skip_bracket(syntax_t *syntax, lexer_t *lex, token_t *tok) {
    if (!_makefile_is_bracket(lex->text[lex->cur])) return;
    char bracket = lex->text[lex->cur];
    do {
        LEX_INCP(1);
        if (_makefile_is_quote(lex->text[lex->cur])) _makefile_skip_quote(syntax, lex, tok);
        if (_makefile_is_bracket(lex->text[lex->cur])) _makefile_skip_bracket(syntax, lex, tok);
    } while (lex->cur < lex->text_sz && !_makefile_match_bracket(bracket, lex->text[lex->cur]));
    LEX_INCP(1);
}

static bool _makefile_is_word(syntax_t *syntax, char c) {
    return strchr("#\'\"`$(){}[]+?:=,", c) == NULL && !isspace(c);
}

static inline bool _makefile_is_num(char c) {
    return strchr("xXbBoO.", c) != NULL || isxdigit(c);
}

static token_t _makefile_next_token(syntax_t *syntax, lexer_t *lex) {
    token_t tok = {0};
    syntax->skip_space(syntax, lex);
    if (lex->cur >= lex->text_sz) return tok;

    tok.start = tok.end = lex->cur;
    if (lex->text[lex->cur] == '#') {
        tok.type = LTK_COMMENT;
        while (lex->text[lex->cur] != '\n' && lex->cur < lex->text_sz) LEX_INC(1);
        return tok;
    }
    if (_makefile_is_quote(lex->text[lex->cur])) {
        tok.type = LTK_LITERAL;
        _makefile_skip_quote(syntax, lex, &tok);
        LEX_INC(1);
        return tok;
    }
next:
    if (isdigit(lex->text[lex->cur])) {
        tok.type = LTK_LITERAL;
        while (_makefile_is_num(lex->text[lex->cur]) && lex->cur < lex->text_sz) LEX_INC(1);
        return tok;
    }
    if (syntax->is_operator(syntax, lex->text[lex->cur])) {
        tok.type = LTK_OPERATOR;
        if (lex->text[lex->cur] == '$' || lex->text[lex->cur] == '-') {
            char c =  lex->text[lex->cur];
            tok.type = LTK_LITERAL;
            LEX_INC(1);
            if (c == '$' && _makefile_is_bracket(lex->text[lex->cur])) {
                _makefile_skip_bracket(syntax, lex, &tok);
                return tok;
            }
            while (syntax->is_word(syntax, lex->text[lex->cur]) && lex->cur < lex->text_sz) LEX_INC(1);
            return tok;
        }
        LEX_INC(1);
        return tok;
    }
    tok.type = LTK_IDENTIFIER;
    do LEX_INC(1)
    while (syntax->is_word(syntax, lex->text[lex->cur]) && lex->cur < lex->text_sz);
    if (_makefile_is_keyword(syntax, lex, tok)) tok.type = LTK_KEYWORD;
    return tok;
}

#undef LEX_INC
#undef LEX_INCP

syntax_t syntax_makefile(void) {
    return (syntax_t) {
        .name        = "makefile",
        .extensions  = _MAKEFILE_EXTENSIONS,
        .keywords    = _MAKEFILE_KEYWORDS,
        .is_operator = _makefile_is_operator,
        .is_word     = _makefile_is_word,
        .next_token  = _makefile_next_token,
        .skip_space  = default_skip_space,
    };
}

#endif
