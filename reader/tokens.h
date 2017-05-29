#ifndef __READER__TOKENS_H__
#define __READER__TOKENS_H__

#include "symbol.h"
#include "ast.h"

enum LispTok {
    ERROR = 256,
    NUM,
    VAR,
    LISPVAL,
};

union yystype {
    // What we want
    LispVal* value;
    // Terminals
    Symbol id;
    int token;
    char err_char;
} yylval;

typedef struct tagged_stype {
    int tag;
    union yystype sval;
} tagged_stype;

#endif /* __READER__TOKENS_H__ */
