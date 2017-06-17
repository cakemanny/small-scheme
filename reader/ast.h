#ifndef __READER__AST_H__
#define __READER__AST_H__

#include "symbol.h"
#include <stdio.h> // FILE*

#define DECL_STRUCT(x) struct x; typedef struct x x
DECL_STRUCT(LispVal );
DECL_STRUCT(List    );

typedef LispVal* (*primfunc)(LispVal* /*args*/);

struct LispVal {
    enum LispTag {
        LATOM,
        LNUM,
        LCONS,
        LNIL,
        LLAM,
        LPRIM,
        LBOOL,
        LERROR,
        LCHAR,
        LMAC,
    } tag;
    union {
        Symbol atom; // LATOM
        int number; // LNUM
        struct { // LCONS
            LispVal* head;
            LispVal* tail;
        };
        struct { // LLAM || LMAC
            LispVal* params;
            LispVal* body;
            LispVal* closure;
        };
        primfunc cfunc; // LPRIM
        _Bool boolean; // LBOOL
        const char* error_msg; // LERROR
        int character; // LCHAR
    };
};


LispVal* lisp_atom(Symbol atom);
LispVal* lisp_num(int number);
LispVal* lisp_cons(LispVal* head, LispVal* tail);
LispVal* lisp_nil();
LispVal* lisp_lam(LispVal* params, LispVal* body, LispVal* closure);
LispVal* lisp_macro(LispVal* params, LispVal* body, LispVal* closure);
LispVal* lisp_prim(primfunc cfunc);
LispVal* lisp_bool(_Bool boolean);
LispVal* lisp_err(const char* error_msg);
LispVal* lisp_char(int character);

void print_lispval(FILE* out, LispVal* value);

const char* lv_tagname(LispVal* value);

#endif /* __READER__AST_H__ */
