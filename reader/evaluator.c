#include "evaluator.h"
#include <assert.h>
#include <string.h>

int debug_evaluator = 0;

LispVal* env; // Global environment

static LispVal* eval_with_env(LispVal* expr, LispVal* env);
static LispVal* eval_quasi(LispVal* template, LispVal* env, int quote_level);

static LispVal* eval_body(LispVal* expressions, LispVal* env)
{
    // Assume good list length > 1
    LispVal* result = lisp_nil(); // just in case
    for (LispVal* e = expressions; e->tag == LCONS; e = e->tail) {
        result =  eval_with_env(e->head, env);
    }
    return result;
}

static LispVal* apply(LispVal* fn, LispVal* args)
{
    if (fn->tag == LLAM || fn->tag == LMAC) {
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
        return eval_body(fn->body, env_w_bound_args);
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

static _Bool is_the_atom(const char* symbol, LispVal* val)
{
    return val->tag == LATOM && sym_equal(val->atom, sym(symbol));
}

static LispVal* eval_each_quasi(LispVal* list, LispVal* env, int quote_level)
{
    if (list->tag == LNIL) {
        return list;
    } else if (list->tag == LCONS) {
        LispVal* head = list->head;
        if (quote_level == 0 && good_list(head)
                && list_length(head) == 2
                && is_the_atom("unquote-splicing", head->head)) {
            // (... (unquote-splicing <val>) ...)
            LispVal* evalled_tail = eval_each_quasi(list->tail, env, 0);
            LispVal* unquoted = eval_with_env(head->tail->head, env);
            if (!good_list(unquoted)) {
                return lisp_err("unquote-splicing must expand to a list");
            }
            if (unquoted->tag == LNIL)
                return evalled_tail;
            // splice evalled_tail onto end of unquoted
            for (LispVal* e = unquoted; e->tag != LNIL; e = e->tail) {
                if (e->tail->tag == LNIL) {
                    e->tail = evalled_tail;
                    break;
                }
            }
            return unquoted;
        }
        return lisp_cons(
                eval_quasi(list->head, env, quote_level),
                eval_each_quasi(list->tail, env, quote_level));
    } else {
        // TODO: error
        fprintf(stderr, "bad list: ");
        print_lispval(stderr, list);
        fputs("\n", stderr);
        return lisp_nil();
    }
}

static LispVal* eval_quasi(LispVal* template, LispVal* env, int quote_level)
{
    if (good_list(template)) {
        if (list_length(template) == 2) {
            if (quote_level == 0) {
                if (is_the_atom("unquote", template->head)) {
                    return eval_with_env(template->tail->head, env);
                } else if (is_the_atom("unquote-splicing", template->head)) {
                    return lisp_err("unquote-splicing must be inside a list");
                }
            } else {
                if (is_the_atom("unquote", template->head)
                        || is_the_atom("unquote-splicing", template->head)) {
                    // decrease quote-level
                    return lisp_cons(
                            template->head, // unquote/unquote-splicing
                            lisp_cons(
                                eval_quasi(template->tail->head, env, quote_level - 1),
                                lisp_nil()));
                } else if (is_the_atom("quasiquote", template->head)) {
                    // increase quote-level
                    return lisp_cons(
                            template->head, // quasiquote
                            lisp_cons(
                                eval_quasi(template->tail->head, env, quote_level + 1),
                                lisp_nil()));
                }
            }
        }

        // Not an unquote or an unquote-splicing but is a list
        return eval_each_quasi(template, env, quote_level);
    }
    // Not a list
    // (later we may need to consider vectors)
    return template;
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
        case LMAC: // The lambda is itself
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
            if (!good_list(expr)) {
                return lisp_err("proper list required for function "
                        "application or macro use");
            }
            // Evaluate a combination
            LispVal* const head = expr->head;
            if (head->tag == LATOM) {
                // Check for special forms
                if (sym_equal(head->atom, sym("lambda"))
                        || sym_equal(head->atom, sym("macro"))) {
                    if (list_length(expr) < 3) {
                        return lisp_err("bad special form");
                    }
                    // TODO: support rest args
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
                    LispVal* body = expr->tail->tail;
                    if (sym_equal(head->atom, sym("macro"))) {
                        return lisp_macro(params, body, env);
                    }
                    return lisp_lam(params, body, env);
                } else if (sym_equal(head->atom, sym("quote"))) {
                    if (list_length(expr) != 2) {
                        return lisp_err("wrong number of arguments to special "
                                "form: quote");
                    }
                    return expr->tail->head;
                } else if (sym_equal(head->atom, sym("if"))) {
                    // (if <test> <consequent> <alternate>)
                    if (list_length(expr) != 4) {
                        return lisp_err("incorrect syntax for if");
                    }
                    LispVal* test_result = eval_with_env(expr->tail->head, env);
                    if (test_result->tag == LBOOL && !test_result->boolean) {
                        // False
                        return eval_with_env(expr->tail->tail->tail->head, env);
                    } else {
                        return eval_with_env(expr->tail->tail->head, env);
                    }
                } else if (sym_equal(head->atom, sym("eval"))) {
                    if (list_length(expr) != 2) {
                        return lisp_err("wrong number of args to eval");
                    }
                    return eval_with_env(
                            eval_with_env(expr->tail->head, env), env);
                } else if (sym_equal(head->atom, sym("begin"))) {
                    return eval_body(expr->tail, env);
                } else if (sym_equal(head->atom, sym("define"))) {
                    // (define <variable> <expression>)
                    // (define (<variable> <formals>) <expression>)
                    if (list_length(expr) != 3) {
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
                } else if (sym_equal(head->atom, sym("quasiquote"))) {
                    if (list_length(expr) != 2) {
                        return lisp_err("wrong number of arguments to special "
                                "form: quasiquote");
                    }
                    return eval_quasi(expr->tail->head, env, 0);
                } else if (sym_equal(head->atom, sym("unquote"))) {
                    return lisp_err("unquote must be in quasiquote");
                } else if (sym_equal(head->atom, sym("unquote-splicing"))) {
                    return lisp_err("unquote-splicing must be in quasiquote");
                } else {
                    // Check if it's a macro!
                    LispVal* op = eval_with_env(expr->head, env);
                    if (op->tag == LMAC) {
                        // we basically want to apply the lambda to the tail
                        // then eval the result
                        // In a compiler, these would be done in two separate
                        // stages I think
                        LispVal* expanded = apply(op, expr->tail);
                        return eval_with_env(expanded, env);
                    }
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

LispVal* prim_print_heap_state(LispVal* args)
{
    void print_heap_state(); // runtime.c
    print_heap_state();
    return lisp_nil();
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

    env = add_prim(sym("print-heap-state"), prim_print_heap_state, env);
}

