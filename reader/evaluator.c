#include "evaluator.h"
#include <assert.h>

int debug_evaluator = 0;

LispVal* env;

static LispVal* eval_with_env(LispVal* expr, LispVal* env);

static LispVal* apply(LispVal* fn, LispVal* args)
{
    if (fn->tag == LLAM) {
        // bind args to fn environment
        LispVal* env_w_bound_args = fn->closure;
        if (debug_evaluator) {
            fprintf(stderr, "env before: ");
            print_lispval(stderr, env_w_bound_args);
            fputs("\n", stderr);
        }
        for (LispVal* p = fn->params, * a = args;
                p->tag == LCONS || a->tag == LCONS;
                p = p->tail, a = a->tail) {
            if (p->tag != LCONS || a->tag != LCONS) {
                fprintf(stderr, "incorrect number of arguments for call "
                        "to lambda\n");
                return lisp_nil();
            }
            env_w_bound_args =
                lisp_cons(lisp_cons(p->head, a->head), env_w_bound_args);
        }
        if (debug_evaluator) {
            fprintf(stderr, "after binding args: ");
            print_lispval(stderr, env_w_bound_args);
            fputs("\n", stderr);
        }
        // eval body
        return eval_with_env(fn->body, env_w_bound_args);
    } else if (fn->tag == LPRIM) {
        return fn->cfunc(args);
    } else {
        // TODO: return error
        fprintf(stderr, "cannot apply non-lambda: ");
        print_lispval(stderr, fn);
        fputs("\n", stderr);
        return lisp_nil();
    }
}

static LispVal* eval_each(LispVal* list, LispVal* env)
{
    if (list->tag == LNIL) {
        return list;
    } else if (list->tag == LCONS) {
        return lisp_cons(
                eval_with_env(list->head, env),
                eval_each(list->tail, env));
    } else {
        // TODO: error
        fprintf(stderr, "bad list: ");
        print_lispval(stderr, list);
        fputs("\n", stderr);
        return lisp_nil();
    }
}

static LispVal* eval_with_env(LispVal* expr, LispVal* env)
{
    if (debug_evaluator) {
        fprintf(stderr, "eval_with_env: ");
        print_lispval(stderr, expr);
        fprintf(stderr, ", env = ");
        print_lispval(stderr, env);
        fputs("\n", stderr);
    }
    switch (expr->tag)
    {
        case LNUM:
        case LNIL:
        case LLAM: // The lambda is itself
        case LPRIM: // The lambda is itself
        case LBOOL:
        case LERROR:
            return expr;
        case LATOM:
        {
            // lookup in the environment
            for (LispVal* e = env; e->tag != LNIL; e = e->tail) {
                LispVal* nvp = e->head; // (name . value)
                if (sym_equal(expr->atom, nvp->head->atom)) {
                    if (debug_evaluator) {
                        fprintf(stderr, "evaluates to: ");
                        print_lispval(stderr, nvp->tail);
                        fprintf(stderr, "\n");
                    }
                    return nvp->tail;
                }
            }
            // TODO: return an error
            fprintf(stderr, "var not found: %s\n", symtext(expr->atom));
            return lisp_nil();
        }
        case LCONS:
        {
            // Evaluate a combination
            LispVal* head = expr->head;
            if (head->tag == LATOM) {
                // Check for special forms
                if (sym_equal(head->atom, sym("lambda"))) {
                    if (expr->tail->tag != LCONS) {
                        fprintf(stderr, "bad special form\n");
                        return lisp_nil();
                    }
                    LispVal* params = expr->tail->head;
                    if (params->tag != LCONS && params->tag != LNIL) {
                        fprintf(stderr, "bad special form: params must be list\n");
                        return lisp_nil();
                    }
                    for (LispVal* p = params; p->tag != LNIL; p = p->tail) {
                        if (p->tag != LCONS) {
                            fprintf(stderr, "bad list\n");
                            return lisp_nil();
                        }
                        LispVal* value = p->head;
                        if (value->tag != LATOM) {
                            fprintf(stderr, "bad special form: lambda params"
                                    "must be atoms\n");
                            return lisp_nil();
                        }
                    }
                    if (expr->tail->tail->tail->tag != LNIL) {
                        fprintf(stderr, "too many args to special form: "
                                "lambda\n");
                        return lisp_nil();
                    }
                    LispVal* body = expr->tail->tail->head;

                    return lisp_lam(params, body, env);
                } else if (sym_equal(head->atom, sym("quote"))) {
                    if (expr->tail->tag != LCONS) {
                        fprintf(stderr, "bad special form\n");
                        return lisp_nil();
                    }
                    if (expr->tail->tail->tag != LNIL) {
                        fprintf(stderr, "wrong number of arguments to special "
                                "form: quote\n");
                        return lisp_nil();
                    }
                    return expr->tail->head;
                }
                // TODO: cond, define
            }
            LispVal* evaluated = eval_each(expr, env);
            assert(evaluated->tag == LCONS);
            return apply(evaluated->head, evaluated->tail);
        }
    }
}

LispVal* eval(LispVal* expr)
{
    return eval_with_env(expr, env);
}

LispVal* prim_plus(LispVal* args)
{
    int result = 0;
    while (args->tag == LCONS) {
        if (args->head->tag == LNUM)
            result += args->head->number;
        else
            return lisp_nil();
        args = args->tail;
    }
    return lisp_num(result);
}

LispVal* prim_multiply(LispVal* args)
{
    int result = 1;
    while (args->tag == LCONS) {
        if (args->head->tag == LNUM)
            result *= args->head->number;
        else
            return lisp_nil();
        args = args->tail;
    }
    return lisp_num(result);
}

// Assume well formed list
int list_length(LispVal* list)
{
    int result = 0;
    for (; list->tag != LNIL; list = list->tail)
        result++;
    return result;
}

LispVal* is_atom(LispVal* args)
{
    return lisp_bool(args->head->tag == LATOM);
}

LispVal* is_number(LispVal* args)
{
    return lisp_bool(args->head->tag == LNUM);
}

LispVal* prim_cons(LispVal* args)
{
    if (list_length(args) != 2) {
        return lisp_err("cons: expected 2 args");
    }
    return lisp_cons(args->head, args->tail->head);
}
LispVal* prim_car(LispVal* args)
{
    if (list_length(args) != 1) {
        return lisp_err("car: expected 1 arg");
    }
    if (args->head->tag != LCONS) {
        return lisp_err("car: invalid type, expected pair");
    }
    return args->head->head;
}

LispVal* prim_cdr(LispVal* args)
{
    if (list_length(args) != 1) {
        return lisp_err("cdr: expected 1 arg");
    }
    if (args->head->tag != LCONS) {
        return lisp_err("cdr: invalid type, expected pair");
    }
    return args->head->tail;
}

LispVal* add_prim(Symbol symbol, primfunc primop, LispVal* env)
{
    return lisp_cons(
            lisp_cons(
                lisp_atom(symbol),
                lisp_prim(primop)),
            env);
}

void initialize_evaluator()
{
    env = lisp_nil();
    // TODO: add more primitive operations
    env = add_prim(sym("+"), prim_plus, env);
    env = add_prim(sym("*"), prim_multiply, env);
    env = add_prim(sym("cons"), prim_cons, env);
    env = add_prim(sym("car"), prim_car, env);
    env = add_prim(sym("cdr"), prim_cdr, env);
    env = add_prim(sym("symbol?"), is_atom, env);
    env = add_prim(sym("number?"), is_number, env);
}

