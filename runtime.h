#ifndef __SS_RUNTIME_H__
#define __SS_RUNTIME_H__

#include "ast.h"

struct Env;
typedef struct Env Env;
struct LispVal;
typedef struct LispVal LispVal;

struct Env {
    Symbol name;
    LispVal* value;
    Env* next;
};

struct LispVal {
    enum LispValTag {
        LAMVAL,
        VARVAL,
        NUMVAL,
        ERRVAL,
    } tag;
    union {
        struct {
            Symbol param;
            Exp* body;
        };
        Symbol var;
        Symbol num;
        const char* err_msg;
    };
    Env* env;
};

/*
 * Evaluate an expression
 */
LispVal* eval(Exp* expr);

/*
 * Print a value
 */
void print_val(FILE* out, LispVal* val);

#endif /* __SS_RUNTIME_H__ */
