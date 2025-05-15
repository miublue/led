#ifndef __parser_h
#define __parser_h

#include <stdbool.h>

enum { LTK_EOF, LTK_LITERAL, LTK_IDENTIFIER, LTK_KEYWORD, LTK_OPERATOR, LTK_COMMENT };
typedef struct { int type, start, end; } token_t;
typedef struct { int cur, text_sz; const char *text; } lexer_t;

typedef struct syntax_t {
    const char *name;
    const char **extensions;
    const char **keywords;
    bool (*is_operator)(struct syntax_t*, char);
    bool (*is_word)(struct syntax_t*, char);
    void (*skip_space)(struct syntax_t*, lexer_t*);
    token_t (*next_token)(struct syntax_t*, lexer_t*);
} syntax_t;

bool default_is_keyword(syntax_t *syntax, lexer_t *lex, token_t tok);
bool default_is_operator(syntax_t *syntax, char c);
bool default_is_word(syntax_t *syntax, char c);
void default_skip_space(syntax_t *syntax, lexer_t *lex);
token_t default_next_token(syntax_t *syntax, lexer_t *lex);

#endif
