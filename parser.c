#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "parser.h"

bool default_is_keyword(syntax_t *syntax, lexer_t *lex, token_t tok) {
    const int sz = tok.end-tok.start;
    const char *ptr = lex->text+tok.start;
    for (int i = 0; syntax->keywords[i]; ++i) {
        if (strlen(syntax->keywords[i]) == sz && !strncmp(syntax->keywords[i], ptr, sz))
            return true;
    }
    return false;
}

bool default_is_operator(syntax_t *syntax, char c) {
    return strchr("+-*/%^~!=<>?:;.,&|(){}[]@$", c) != NULL;
}

bool default_is_word(syntax_t *syntax, char c) {
    return !(isspace(c) || syntax->is_operator(syntax, c) || c == '\'' || c == '\"' || c == '`');
}

void default_skip_space(syntax_t *syntax, lexer_t *lex) {
    while (isspace(lex->text[lex->cur]) && lex->cur < lex->text_sz) ++lex->cur;
}

#define LEX_INC(AMOUNT) { tok.end += AMOUNT; lex->cur += AMOUNT; }

static inline bool _is_num(char c) {
    return strchr("xX", c) != NULL || isxdigit(c);
}

token_t default_next_token(syntax_t *syntax, lexer_t *lex) {
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
        while (_is_num(lex->text[lex->cur]) && lex->cur < lex->text_sz) LEX_INC(1);
        return tok;
    }
    if (syntax->is_operator(syntax, lex->text[lex->cur])) {
        tok.type = LTK_OPERATOR;
        LEX_INC(1);
        return tok;
    }
    tok.type = LTK_IDENTIFIER;
    do LEX_INC(1) while (syntax->is_word(syntax, lex->text[lex->cur]) && lex->cur < lex->text_sz);
    if (default_is_keyword(syntax, lex, tok) || lex->text[tok.start] == '#') tok.type = LTK_KEYWORD;
    return tok;
}

#undef LEX_INC
