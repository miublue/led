#ifndef __SYNTAX_SH
#define __SYNTAX_SH

#include <string.h>
#include <ctype.h>
#include "parser.h"

static const char *_SH_EXTENSIONS[] = {".sh", ".profile", "shrc", NULL};
static const char *_SH_KEYWORDS[] = {
    // sh builtins
    "break", "cd", "continue", "eval", "exec", "exit",
    "export", "getopts", "hash", "pwd", "readonly", "return",
    "shift", "test", "times", "trap", "umask", "unset",
    // sh keywords
    "if", "then", "elif", "else", "fi", "time",
    "for", "in", "until", "while", "do", "done",
    "case", "esac", "coproc", "select", "function",
    // sh commands
    "alloc", "autoload", "bind", "bindkey", "builtin", "bye",
    "caller", "cap", "chdir", "clone", "comparguments", "compcall",
    "compctl", "compdescribe", "compfiles", "compgen", "compgroups",
    "complete", "compquote", "comptags", "comptry", "compvalues",
    "declare", "dirs", "disable", "disown", "dosh", "echotc", "echoti",
    "help", "history", "hist", "let", "local", "login", "logout", "map",
    "mapfile", "popd", "print", "pushd", "readarray", "repeat", "savehistory",
    "source", "shopt", "stop", "suspend", "typeset", "whence",

    "alias", "bg", "command", "false", "fc", "fg", "getopts", "hash", "jobs",
    "kill", "newgrp", "pwd", "read", "true", "umask", "unalias", "wait", NULL
};

/*
static bool _sh_is_operator(syntax_t *syntax, char c) {
    return strchr("+-/*%^~!=<>?:;.,&|(){}[]@$", c) != NULL;
}
*/

#define LEX_INC(AMOUNT) { tok.end += AMOUNT; lex->cur += AMOUNT; }
#define LEX_INCP(AMOUNT) { tok->end += AMOUNT; lex->cur += AMOUNT; }

static inline bool _sh_is_num(char c) {
    return strchr("xXbBoO._", c) != NULL || isxdigit(c);
}

static inline bool _sh_is_bracket(char c) {
    return strchr("({[", c) != NULL;
}

static inline bool _sh_match_bracket(char bracket, char c) {
    if (bracket == '(') return c == ')';
    if (bracket == '{') return c == '}';
    if (bracket == '[') return c == ']';
    return false;
}

static inline bool _sh_is_quote(char c) {
    return strchr("\"'`", c) != NULL;
}

static bool _sh_is_word(syntax_t *syntax, char c) {
    return strchr("#\'\"`$:;!=(){}[]+/*%~^|&", c) == NULL && !isspace(c);
}

static inline void _sh_skip_quote(syntax_t *syntax, lexer_t *lex, token_t *tok) {
    if (!_sh_is_quote(lex->text[lex->cur])) return;
    char quote = lex->text[lex->cur];
    if (lex->cur > 0 && lex->text[lex->cur-1] == '\\') return;
    do {
        int amnt = (lex->text[tok->end] == '\\')? 2 : 1;
        LEX_INCP(amnt);
    } while (lex->cur < lex->text_sz && lex->text[lex->cur] != quote);
}

static inline void _sh_skip_bracket(syntax_t *syntax, lexer_t *lex, token_t *tok) {
    if (!_sh_is_bracket(lex->text[lex->cur])) return;
    char bracket = lex->text[lex->cur];
    do {
        LEX_INCP(1);
        if (_sh_is_quote(lex->text[lex->cur])) _sh_skip_quote(syntax, lex, tok);
        else if (_sh_is_bracket(lex->text[lex->cur])) _sh_skip_bracket(syntax, lex, tok);
    } while (lex->cur < lex->text_sz && !_sh_match_bracket(bracket, lex->text[lex->cur]));
    LEX_INCP(1);
}

static token_t _sh_next_token(syntax_t *syntax, lexer_t *lex) {
    token_t tok = {0};
    syntax->skip_space(syntax, lex);
    if (lex->cur >= lex->text_sz) return tok;

    tok.start = tok.end = lex->cur;
    if (lex->text[lex->cur] == '#') {
        tok.type = LTK_COMMENT;
        while (lex->text[lex->cur] != '\n' && lex->cur < lex->text_sz) LEX_INC(1);
        return tok;
    }
    if (_sh_is_quote(lex->text[lex->cur])) {
        tok.type = LTK_LITERAL;
        _sh_skip_quote(syntax, lex, &tok);
        LEX_INC(1);
        return tok;
    }
next:
    if (isdigit(lex->text[lex->cur])) {
        tok.type = LTK_LITERAL;
        while (_sh_is_num(lex->text[lex->cur]) && lex->cur < lex->text_sz) LEX_INC(1);
        return tok;
    }
    if (syntax->is_operator(syntax, lex->text[lex->cur])) {
        tok.type = LTK_OPERATOR;
        if (lex->text[lex->cur] == '$' || lex->text[lex->cur] == '-') {
            char c =  lex->text[lex->cur];
            tok.type = LTK_LITERAL;
            LEX_INC(1);
            if (c == '$' && _sh_is_bracket(lex->text[lex->cur])) {
                _sh_skip_bracket(syntax, lex, &tok);
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
    if (default_is_keyword(syntax, lex, tok)) tok.type = LTK_KEYWORD;
    return tok;
}

#undef LEX_INC
#undef LEX_INCP

syntax_t syntax_sh(void) {
    return (syntax_t) {
        .name        = "sh",
        .extensions  = _SH_EXTENSIONS,
        .keywords    = _SH_KEYWORDS,
        .is_operator = default_is_operator,
        .is_word     = _sh_is_word,
        .next_token  = _sh_next_token,
        .skip_space  = default_skip_space,
    };
}

#endif
