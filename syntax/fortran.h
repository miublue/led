#ifndef __SYNTAX_FORTRAN
#define __SYNTAX_FORTRAN

#include "parser.h"

/* XXX: keywords that are actually multiple words (???? i hate fortran) */
static const char *_FORTRAN_EXTENSIONS[] = {".f", ".for", ".ftn", ".f77", ".f90", ".f95", ".f03", ".f08", NULL};
static const char *_FORTRAN_KEYWORDS[] = {
    // FORTRAN 77
    "assign", "backspace", "block", "call", "close", "common", "continue", "data",
    "dimension", "do", "else", "end", "endfile", "endif", "entry", "equivalence",
    "external", "format", "function", "goto", "if", "implicit", "inquire", "intrinsic",
    "open", "parameter", "pause", "print", "program", "read", "return", "rewind", "rewrite",
    "save", "stop", "subroutine", "then", "write",
    // FORTRAN 90
    "allocatable", "allocate", "case", "contains", "cycle", "deallocate", "elsewhere", "exit?",
    "include", "interface", "intent", "module", "namelist", "nullify", "only", "operator",
    "optional", "pointer", "private", "procedure", "public", "recursive", "result", "select",
    "sequence", "target", "use", "while", "where",
    // FORTRAN 95
    "elemental", "forall", "pure",
    // FORTRAN 03
    "abstract", "associate", "asynchronous", "bind", "class", "deferred", "enum", "enumerator",
    "extends", "final", "flush", "generic", "import", "non_overridable", "nopass",
    "pass", "protected", "value", "volatile", "wait",
    // FORTRAN 08
    "block", "codimension", "concurrent", "contiguous", "critical", "error", "submodule",
    "sync", "all", "images", "memory", "lock", "unlock", "impure",
    // "sync all", "sync images", "sync memory", weaeweawewaewaea
    // TYPES
    "type", "structure", "record", "character", "integer", "real", "complex", "logical",
    "byte", "double",
    NULL
};

#define LEX_INC(AMOUNT) { tok.end += AMOUNT; lex->cur += AMOUNT; }

static inline bool _fortran_is_keyword(syntax_t *syntax, lexer_t *lex, token_t tok) {
    const int sz = tok.end-tok.start;
    const char *ptr = lex->text+tok.start;
    for (int i = 0; syntax->keywords[i]; ++i) {
        if (strlen(syntax->keywords[i]) == sz && !strncasecmp(syntax->keywords[i], ptr, sz))
            return true;
    }
    return false;
}

static inline bool _fortran_is_num(char c) {
    return strchr("xXbBoO._", c) != NULL || isxdigit(c);
}

static inline bool _fortran_is_operator(syntax_t *syntax, char c) {
    return strchr("+-*/%^~!=<>:.,&|(){}[]", c) != NULL;
}

static token_t _fortran_next_token(syntax_t *syntax, lexer_t *lex) {
    token_t tok = {0};
    syntax->skip_space(syntax, lex);
    if (lex->cur >= lex->text_sz) return tok;

    tok.start = tok.end = lex->cur;
    if (lex->cur < lex->text_sz && lex->text[lex->cur] == '!') {
        tok.type = LTK_COMMENT;
        while (lex->text[lex->cur] != '\n' && lex->cur < lex->text_sz) LEX_INC(1);
        return tok;
    }

    if (lex->text[lex->cur] == '\"' || lex->text[lex->cur] == '\'') {
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
        while (_fortran_is_num(lex->text[lex->cur]) && lex->cur < lex->text_sz) LEX_INC(1);
        return tok;
    }
    if (syntax->is_operator(syntax, lex->text[lex->cur])) {
        tok.type = LTK_OPERATOR;
        LEX_INC(1);
        return tok;
    }
    tok.type = LTK_IDENTIFIER;
    do LEX_INC(1) while (syntax->is_word(syntax, lex->text[lex->cur]) && lex->cur < lex->text_sz);
    if (_fortran_is_keyword(syntax, lex, tok)) tok.type = LTK_KEYWORD;
    return tok;
}

#undef LEX_INC

syntax_t syntax_fortran(void) {
    return (syntax_t) {
        .name        = "fortran",
        .extensions  = _FORTRAN_EXTENSIONS,
        .keywords    = _FORTRAN_KEYWORDS,
        .is_operator = default_is_operator,
        .is_word     = default_is_word,
        .next_token  = _fortran_next_token,
        .skip_space  = default_skip_space,
    };
}
#endif
