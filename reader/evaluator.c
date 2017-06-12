#include "evaluator.h"
#include <assert.h>
#include <string.h>

int debug_evaluator = 0;

LispVal* env; // Global environment

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
                return lisp_err("incorrect number of arguments "
                        "for call to lambda");
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

static _Bool good_list(LispVal* list)
{
    for (; list->tag != LNIL; list = list->tail)
        if (list->tag != LCONS)
            return 0;
    return 1;
}

// Assume well formed list
static int list_length(LispVal* list)
{
    int result = 0;
    for (; list->tag != LNIL; list = list->tail)
        result++;
    return result;
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
        case LCHAR:
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
            LispVal* const head = expr->head;
            if (head->tag == LATOM) {
                // Check for special forms
                if (sym_equal(head->atom, sym("lambda"))) {
                    if (!good_list(expr) || list_length(expr) != 3) {
                        return lisp_err("bad special form");
                    }
                    LispVal* params = expr->tail->head;
                    if (!good_list(params)) {
                        return lisp_err("bad special form: params must be list");
                    }
                    for (LispVal* p = params; p->tag != LNIL; p = p->tail) {
                        if (p->head->tag != LATOM) {
                            return lisp_err("bad special form: lambda params"
                                    "must be atoms");
                        }
                    }
                    LispVal* body = expr->tail->tail->head;
                    return lisp_lam(params, body, env);
                } else if (sym_equal(head->atom, sym("quote"))) {
                    if (!good_list(expr)) {
                        return lisp_err("bad special form: quote");
                    }
                    if (list_length(expr) != 2) {
                        return lisp_err("wrong number of arguments to special "
                                "form: quote");
                    }
                    return expr->tail->head;
                } else if (sym_equal(head->atom, sym("if"))) {
                    // (if <test> <consequent> <alternate>)
                    if (!(good_list(expr) && list_length(expr) == 4)) {
                        return lisp_err("incorrect syntax for if");
                    }
                    LispVal* test_result = eval_with_env(expr->tail->head, env);
                    if (test_result->tag == LBOOL && !test_result->boolean) {
                        // False
                        return eval_with_env(expr->tail->tail->tail->head, env);
                    } else {
                        return eval_with_env(expr->tail->tail->head, env);
                    }
                } else if (sym_equal(head->atom, sym("define"))) {
                    // (define <variable> <expression>)
                    // (define (<variable> <formals>) <expression>)
                    if (!good_list(expr) || list_length(expr) != 3) {
                        return lisp_err("bad special form: define");
                    }
                    if (expr->tail->head->tag == LATOM) {
                        LispVal* varname = expr->tail->head;
                        LispVal* value =
                            eval_with_env(expr->tail->tail->head, env);

                        // save a copy of env, overwrite env with our new
                        // definition and set the tail to be the saved copy
                        LispVal* saved_env = lisp_cons(env->head, env->tail);
                        env->head = lisp_cons(varname, value);
                        env->tail = saved_env;
                        return env->head;
                    }
                    if (good_list(expr->tail->head)) {
                        LispVal* var_and_formals = expr->tail->head;
                        if (var_and_formals->head->tag == LATOM) {
                            LispVal* lambda =
                                lisp_cons(lisp_atom(sym("lambda")),
                                        lisp_cons(var_and_formals->tail,
                                            expr->tail->tail));
                            return eval_with_env(
                                    lisp_cons(head, // define
                                        lisp_cons(var_and_formals->head, // <var>
                                            lisp_cons(lambda,
                                                lisp_nil()))), env);
                        }
                    }
                    return lisp_err("bad special form: define");
                }
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
            return lisp_err("+: invalid type, expected number");
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
            return lisp_err("*: invalid type, expected number");
        args = args->tail;
    }
    return lisp_num(result);
}

LispVal* prim_subtract(LispVal* args)
{
    if (args->tag != LCONS)
        return lisp_err("-: expected at least 1 arg");
    if (args->head->tag != LNUM)
        return lisp_err("-: invalid type, expected number");
    int result = args->head->number;
    args = args->tail;

    if (args->tag == LCONS) {
        while (args->tag == LCONS) {
            if (args->head->tag == LNUM)
                result -= args->head->number;
            else
                return lisp_err("-: invalid type, expected number");
            args = args->tail;
        }
        return lisp_num(result);
    } else {
        return lisp_num(-result);
    }
}


// type tests

LispVal* is_bool(LispVal* args)
{
    return lisp_bool(args->head->tag == LBOOL);
}

LispVal* is_atom(LispVal* args)
{
    return lisp_bool(args->head->tag == LATOM);
}

LispVal* is_procedure(LispVal* args)
{
    return lisp_bool(args->head->tag == LLAM || args->head->tag == LPRIM);
}

LispVal* is_pair(LispVal* args)
{
    return lisp_bool(args->head->tag == LCONS);
}

LispVal* is_number(LispVal* args)
{
    return lisp_bool(args->head->tag == LNUM);
}

LispVal* is_char(LispVal* args)
{
    return lisp_bool(args->head->tag == LCHAR);
}
// vector, string, port


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

LispVal* prim_eqv(LispVal* args)
{
    if (list_length(args) != 2) {
        return lisp_err("eqv?: expected 2 args");
    }
    // eqv? sounds like it has the properties of a memcmp
    // 
    return lisp_bool(
            memcmp(args->head, args->tail->head, sizeof *args->head) == 0);
}

_Bool help_equal(LispVal* left, LispVal* right)
{
    if (left->tag == right->tag) {
        if (left->tag == LCONS) {
            if (!help_equal(left->head, right->head)) {
                return 0;
            }
            return help_equal(left->tail, right->tail);
        }
        // TODO: strcmp for strings
        return memcmp(left, right, sizeof *left) == 0;
    }
    return 0;
}

LispVal* prim_equal(LispVal* args)
{
    if (list_length(args) != 2) {
        return lisp_err("eqv?: expected 2 args");
    }
    LispVal* left = args->head;
    LispVal* right = args->tail->head;
    return lisp_bool(help_equal(left, right));
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
    env = add_prim(sym("char?"), is_char, env);
    env = add_prim(sym("boolean?"), is_bool, env);
    env = add_prim(sym("symbol?"), is_atom, env);
    env = add_prim(sym("procedure?"), is_procedure, env);
    env = add_prim(sym("pair?"), is_pair, env);
    env = add_prim(sym("number?"), is_number, env);
    env = add_prim(sym("eqv?"), prim_eqv, env);
    env = add_prim(sym("eq?"), prim_eqv, env);
    env = add_prim(sym("equal?"), prim_equal, env); // this doesn't really need to be prim
    env = add_prim(sym("+"), prim_plus, env);
    env = add_prim(sym("*"), prim_multiply, env);
    env = add_prim(sym("-"), prim_subtract, env);
    env = add_prim(sym("cons"), prim_cons, env);
    env = add_prim(sym("car"), prim_car, env);
    env = add_prim(sym("cdr"), prim_cdr, env);
}

