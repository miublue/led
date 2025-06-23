#ifndef __SYNTAX_ASM
#define __SYNTAX_ASM

#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"

static const char *_ASM_EXTENSIONS[] = {".asm", ".as", ".s", NULL};
static const char *_ASM_KEYWORDS[] = { NULL };

#define LEX_INC(AMOUNT) { tok.end += AMOUNT; lex->cur += AMOUNT; }
#define LEX_INCP(AMOUNT) { tok->end += AMOUNT; lex->cur += AMOUNT; }

static inline bool _asm_is_operator(syntax_t *syntax, char c) {
    return strchr("+-*/^~!=<>;,&|(){}[]@", c) != NULL;
}

static inline bool _asm_is_word(syntax_t *syntax, char c) {
    return !(isspace(c) || syntax->is_operator(syntax, c) || c == '\'' || c == '\"' || c == '`');
}

static inline bool _asm_is_num(char c) {
    return strchr("xXbBoO._", c) != NULL || isxdigit(c);
}

static inline bool _asm_skip_comment(lexer_t *lex, token_t *tok) {
    if (lex->text[lex->cur] == '#' || lex->text[lex->cur] == ';') {
        tok->type = LTK_COMMENT;
        while (lex->cur < lex->text_sz && lex->text[lex->cur] != '\n') LEX_INCP(1);
        return TRUE;
    }
    if (lex->cur+2 < lex->text_sz && !strncmp(lex->text+lex->cur, "//", 2)) {
        tok->type = LTK_COMMENT;
        while (lex->cur < lex->text_sz && lex->text[lex->cur] != '\n') LEX_INCP(1);
        return TRUE;
    }
    if (lex->cur+2 < lex->text_sz && !strncmp(lex->text+lex->cur, "/*", 2)) {
        tok->type = LTK_COMMENT;
        while (lex->cur+2 < lex->text_sz && strncmp(lex->text+lex->cur, "*/", 2) != 0)
            LEX_INCP(1);
        LEX_INCP(2);
        return TRUE;
    }
    return FALSE;
}

static token_t _asm_next_token(syntax_t *syntax, lexer_t *lex) {
    token_t tok = {0};
    syntax->skip_space(syntax, lex);
    if (lex->cur >= lex->text_sz) return tok;

    tok.start = tok.end = lex->cur;
    if (_asm_skip_comment(lex, &tok)) return tok;

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
        while (_asm_is_num(lex->text[lex->cur]) && lex->cur < lex->text_sz) LEX_INC(1);
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
    if (lex->text[tok.start] == '$' || lex->text[tok.start] == '%') tok.type = LTK_LITERAL;
    else tok.type = LTK_KEYWORD;
    return tok;
}

#undef LEX_INCP
#undef LEX_INC

syntax_t syntax_asm(void) {
    return (syntax_t) {
        .name        = "asm",
        .extensions  = _ASM_EXTENSIONS,
        .keywords    = _ASM_KEYWORDS,
        .is_operator = _asm_is_operator,
        .is_word     = _asm_is_word,
        .next_token  = _asm_next_token,
        .skip_space  = default_skip_space,
    };
}

#endif
