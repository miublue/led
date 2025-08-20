#ifndef __SYNTAX_GO
#define __SYNTAX_GO

#include "parser.h"

static const char *_GO_EXTENSIONS[] = {".go", NULL};
static const char *_GO_KEYWORDS[] = {
    "break", "default", "func", "interface", "select",
    "case", "defer", "go", "map", "struct", "chan",
    "else", "goto", "package", "switch", "const",
    "fallthrough", "if", "range", "type", "continue",
    "for", "import", "return", "var",
    // TYPES
    "bool", "string", "rune", "byte", "uintptr",
    "uint", "uint8", "uint16", "uint32", "uint64",
    "int", "int8", "int16", "int32" "int64",
    "float32", "float64", "complex64", "complex128",
    NULL
};

#define LEX_INC(AMOUNT) { tok.end += AMOUNT; lex->cur += AMOUNT; }

static inline bool _go_is_num(char c) {
    return strchr("xXbBoO._", c) != NULL || isxdigit(c);
}

static token_t _go_next_token(syntax_t *syntax, lexer_t *lex) {
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
        while (_go_is_num(lex->text[lex->cur]) && lex->cur < lex->text_sz) LEX_INC(1);
        return tok;
    }
    if (syntax->is_operator(syntax, lex->text[lex->cur])) {
        tok.type = LTK_OPERATOR;
        LEX_INC(1);
        return tok;
    }
    tok.type = LTK_IDENTIFIER;
    do LEX_INC(1) while (syntax->is_word(syntax, lex->text[lex->cur]) && lex->cur < lex->text_sz);
    if (default_is_keyword(syntax, lex, tok) || lex->text[tok.start] == '@') tok.type = LTK_KEYWORD;
    return tok;
}

#undef LEX_INC

syntax_t syntax_go(void) {
    return (syntax_t) {
        .name        = "go",
        .extensions  = _GO_EXTENSIONS,
        .keywords    = _GO_KEYWORDS,
        .is_operator = default_is_operator,
        .is_word     = default_is_word,
        .next_token  = _go_next_token,
        .skip_space  = default_skip_space,
    };
}
#endif
