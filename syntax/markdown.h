#ifndef __SYNTAX_MARKDOWN
#define __SYNTAX_MARKDOWN

#include <string.h>
#include <ctype.h>
#include "parser.h"

static const char *_MARKDOWN_EXTENSIONS[] = {".md", NULL};
static const char *_MARKDOWN_KEYWORDS[] = {NULL};

#define LEX_INC(AMOUNT) { tok.end += AMOUNT; lex->cur += AMOUNT; }
#define LEX_INCP(AMOUNT) { tok->end += AMOUNT; lex->cur += AMOUNT; }

static const char *_markdown_modifiers[] = {
    "**", "__", "~~", "*", "_",
};

static inline int _markdown_is_modifier(lexer_t *lex) {
    for (int i = 0; i < LENGTH(_markdown_modifiers); ++i) {
        const char *mod = _markdown_modifiers[i];
        const int len = strlen(mod);
        if (lex->cur+len < lex->text_sz && strncmp(lex->text+lex->cur, mod, len) == 0)
            return i;
    }
    return -1;
}

static token_t _markdown_next_token(syntax_t *syntax, lexer_t *lex) {
    token_t tok = {0};
    syntax->skip_space(syntax, lex);
    if (lex->cur >= lex->text_sz) return tok;

    tok.start = tok.end = lex->cur;
    if (lex->text[lex->cur] == '#') {
        tok.type = LTK_KEYWORD;
        while (lex->cur < lex->text_sz && lex->text[lex->cur] != '\n') LEX_INC(1);
        return tok;
    }

    if (lex->cur+3 < lex->text_sz && strncmp(lex->text+lex->cur, "```", 3) == 0) {
        tok.type = LTK_LITERAL;
        do LEX_INC(1)
        while (lex->cur+3 < lex->text_sz && strncmp(lex->text+lex->cur, "```", 3) != 0);
        LEX_INC(3);
        return tok;
    }

    int mod = -1;
    if (lex->cur+2 < lex->text_sz && (mod = _markdown_is_modifier(lex)) != -1) {
        const int len = strlen(_markdown_modifiers[mod]), pos = lex->cur;
        tok.type = LTK_LITERAL;
        LEX_INC(len);
        while (lex->cur+len < lex->text_sz && _markdown_is_modifier(lex) != mod) {
            if (lex->text[lex->cur] == '\n') {
                lex->cur = tok.end = pos+len;
                tok.type = LTK_OPERATOR;
                return tok;
            }
            LEX_INC(1);
        }
        LEX_INC(len);
        return tok;
    }

    if (strchr("!+-", lex->text[lex->cur])) {
        tok.type = LTK_OPERATOR;
        LEX_INC(1);
        return tok;
    }

    if (lex->cur+2 < lex->text_sz && lex->text[lex->cur] == '[') {
        const int pos = lex->cur;
        tok.type = LTK_LITERAL;
        while (lex->cur < lex->text_sz && lex->text[lex->cur] != ']') {
            if (lex->text[lex->cur] == '\n') goto bruh;
            LEX_INC(1);
        }
        LEX_INC(1);
        if (lex->text[lex->cur] == '(') {
            while (lex->cur < lex->text_sz && lex->text[lex->cur] != ')') {
                if (lex->text[lex->cur] == '\n') goto bruh;
                LEX_INC(1);
            }
            LEX_INC(1);
        } else goto bruh;
        return tok;
bruh:
        lex->cur = tok.end = pos;
    }

    tok.type = LTK_IDENTIFIER;
    do LEX_INC(1)
    while (syntax->is_word(syntax, lex->text[lex->cur]) && lex->cur < lex->text_sz);
    return tok;
}

#undef LEX_INC
#undef LEX_INCP

syntax_t syntax_markdown(void) {
    return (syntax_t) {
        .name        = "markdown",
        .extensions  = _MARKDOWN_EXTENSIONS,
        .keywords    = _MARKDOWN_KEYWORDS,
        .is_operator = default_is_operator,
        .is_word     = default_is_word,
        .next_token  = _markdown_next_token,
        .skip_space  = default_skip_space,
    };
}

#endif
