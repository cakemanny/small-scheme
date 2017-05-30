#include "evaluator.h"
#include <assert.h>

int debug_evaluator = 1;

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

void initialize_evaluator()
{
    env = lisp_nil();
    // TODO: add primitive operations
}

