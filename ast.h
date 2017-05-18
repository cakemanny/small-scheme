#ifndef __SS_AST_H__
#define __SS_AST_H__

#include "symbol.h"
#include <stdio.h>

struct Exp;
typedef struct Exp Exp;
struct Exp {
    enum ExpTag {
        APPEXP,
        LAMEXP,
        VAREXP,
        NUMEXP
    } tag;
    union {
        struct { /* APPEXP */
            Exp* left;
            Exp* right;
        };
        struct { /* LAMEXP */
            Symbol param;
            Exp* body;
        };
        Symbol var; /* VAREXP */
        Symbol num; /* NUMEXP */
    };
};

Exp* mkapp(Exp* left, Exp* right);
Exp* mklam(Symbol param, Exp* body);
Exp* mkvar(Symbol var);
Exp* mknum(Symbol num);

void print_exp(FILE* out, Exp* exp);


#endif /* __SS_AST_H__ */
