#define _GNU_SOURCE
#include "ast.h"
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>


static LispVal* lispval(enum LispValTag tag)
{
    LispVal* result = malloc(sizeof *result);
    result->tag = tag;
    return result;
}

static LispVal* lisp_lam(Symbol param, Exp* body, Env* env)
{
    LispVal* result = lispval(LAMVAL);
    result->param = param;
    result->body = body;
    result->env = env;
    return result;
}
static LispVal* lisp_var(Symbol var, Env* env)
{
    LispVal* result = lispval(VARVAL);
    result->var = var;
    result->env = env;
    return result;
}
static LispVal* lisp_num(Symbol num, Env* env)
{
    LispVal* result = lispval(NUMVAL);
    result->num = num;
    result->env = env;
    return result;
}
static LispVal* lisp_err(const char* err_msg, Env* env)
{
    LispVal* result = lispval(ERRVAL);
    result->err_msg = err_msg;
    result->env = env;
    return result;
}

Env* env_cons(Symbol name, LispVal* value, Env* env)
{
    Env* result = malloc(sizeof * result);
    result->name = name;
    result->value = value;
    result->next = env;
    return result;
}

LispVal* eval_with_env(Exp* exp, Env* env);

LispVal* apply(LispVal* fn, LispVal* arg)
{
    if (fn->tag == LAMVAL) {
        Env* env_with_bound_params = env_cons(fn->param, arg, fn->env);
        return eval_with_env(fn->body, env_with_bound_params);
    } else if (fn->tag == ERRVAL) {
        return fn; // propagate error
    } else {
        // TODO: have builtin functions
        return lisp_err("expression is not a function", NULL);
    }
}

LispVal* eval_with_env(Exp* exp, Env* env)
{
    switch (exp->tag)
    {
        case APPEXP:
        {
            // Apply left to right
            LispVal* left_val = eval_with_env(exp->left, env);
            LispVal* right_val = eval_with_env(exp->right, env);
            return apply(left_val, right_val);
        }
        case LAMEXP:
        {
            return lisp_lam(exp->param, exp->body, env);
        }
        case VAREXP:
        {
            for (Env* e = env; e; e = e->next) {
                if (sym_equal(exp->var, e->name)) {
                    return e->value;
                }
            }
            char* msg;
            asprintf(&msg, "var not found: %s" , symtext(exp->var));
            return lisp_err(msg, env);
        }
        case NUMEXP:
        {
            return lisp_num(exp->num, env);
        }
    }
}

LispVal* eval(Exp* exp)
{
    // Create initial environment
    // Evaluate using said environment
    return eval_with_env(exp, 0);
}

void print_val(FILE* out, LispVal* val)
{
    switch (val->tag)
    {
        case LAMVAL:
            fprintf(out, "(lambda (%s) ", symtext(val->param));
            print_exp(out, val->body);
            fputc(')', out);
            break;
        case VARVAL:
            fprintf(out, "%s", symtext(val->var));
            break;
        case NUMVAL:
            fprintf(out, "%s", symtext(val->num));
            break;
        case ERRVAL:
            fprintf(out, "%s", val->err_msg);
            break;
    }
}


