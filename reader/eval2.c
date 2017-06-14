#include "eval2.h"
#include <stdlib.h>

// define routine values such that they cannot be memory addresses
// i.e. not aligned
#define ROUTINE(x)  (((x) << 3LL) | 1LL)

#define DONE                        ROUTINE( 0LL)
#define EVAL_DISPATCH               ROUTINE( 1LL)
#define APPLY_DISPATCH              ROUTINE( 2LL)
#define EV_SELF_EVAL                ROUTINE( 3LL)
#define EV_VARIABLE                 ROUTINE( 4LL)
#define EV_QUOTED                   ROUTINE( 5LL)
#define EV_ASSIGNMENT               ROUTINE( 6LL)
#define EV_DEFINITION               ROUTINE( 7LL)
#define EV_DEFINITION_1             ROUTINE( 8LL)
#define EV_IF                       ROUTINE( 9LL)
#define EV_IF_DECIDE                ROUTINE(10LL)
#define EV_IF_ALTERNATE             ROUTINE(11LL)
#define EV_IF_CONSEQUENT            ROUTINE(12LL)
#define EV_LAMBDA                   ROUTINE(13LL)
#define EV_BEGIN                    ROUTINE(14LL)
#define EV_APPLICATION              ROUTINE(15LL)
#define EVAL_ARGS                   ROUTINE(16LL)
#define EVAL_ARG_LOOP               ROUTINE(17LL)
#define ACCUMULATE_ARG              ROUTINE(18LL)
#define EVAL_LAST_ARG               ROUTINE(19LL)
#define ACCUMULATE_LAST_ARG         ROUTINE(20LL)
#define PRIMITIVE_APPLY             ROUTINE(21LL)
#define COMPOUND_APPLY              ROUTINE(22LL)
#define COMPOUND_APPLY_CONT         ROUTINE(23LL)
#define EXTEND_ENV_LOOP             ROUTINE(24LL)
#define EV_SEQUENCE                 ROUTINE(25LL)
#define EV_SEQUENCE_CONT            ROUTINE(26LL)
#define EV_SEQUENCE_LAST_EXP        ROUTINE(27LL)

#define UNKNOWN_EXPR_ERROR          ROUTINE(98LL)
#define UNKNOWN_PROC_TYPE_ERROR     ROUTINE(99LL)

int debug_eval2 = 0;

LispVal* expr2; // expression to be evaluted
LispVal* env2; // evaluation environment
LispVal* fun2; // procedure to be applied
LispVal* argl2; // list of evaluated arguments
static long long continue2 = DONE; // place to go next
LispVal* val2; // result of evaluation
LispVal* unev2; // temporary register

static long long pc = 0;

#define LISP_STACK_SIZE (128 * 1024)

typedef union StackVal {
    long long routine;
    LispVal* value;
} StackVal;
StackVal stack2[LISP_STACK_SIZE];
StackVal* sp = stack2;

static void save_value(LispVal* lv)
{
    if (sp >= stack2 + LISP_STACK_SIZE) {
        fprintf(stderr, "lisp stack overflow\n");
        exit(EXIT_FAILURE);
    }
    StackVal sv = { .value = lv };
    *sp++ = sv;
}
static void save_location(long long routine)
{
    if (sp >= stack2 + LISP_STACK_SIZE) {
        fprintf(stderr, "lisp stack overflow\n");
        exit(EXIT_FAILURE);
    }
    StackVal sv = { .routine = routine };
    *sp++ = sv;
}
#define save(x) _Generic((x), LispVal*: save_value, default: save_location)(x)

static void restore_value(LispVal** pval)
{
    if (sp <= stack2) {
        fprintf(stderr, "lisp stack underflow\n");
        exit(EXIT_FAILURE);
    }
    StackVal sv = *--sp;
    *pval = sv.value;
}
static void restore_location(long long* ploc)
{
    if (sp <= stack2) {
        fprintf(stderr, "lisp stack underflow\n");
        exit(EXIT_FAILURE);
    }
    StackVal sv = *--sp;
    *ploc = sv.routine;
}
#define restore(x) \
    _Generic((x), LispVal**: restore_value, default: restore_location)(x)


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


static _Bool is_self_evaluating(LispVal* expr)
{
    switch (expr->tag) {
        case LNUM:
        case LNIL:
        case LLAM:
        case LPRIM:
        case LBOOL:
        case LERROR:
        case LCHAR:
            return 1;
        case LATOM:
        case LCONS:
            return 0;
    }
}

static _Bool is_variable(LispVal* expr)
{
    return expr->tag == LATOM;
}

static _Bool is_form(LispVal* expr, const char* formname)
{
    return expr->tag == LCONS
        && expr->head->tag == LATOM
        && sym_equal(expr->head->atom, sym(formname));
}

static _Bool is_quoted(LispVal* expr)
{
    return is_form(expr, "quote");
}

static _Bool is_assignment(LispVal* expr)
{
    return is_form(expr, "set!");
}

static _Bool is_definition(LispVal* expr)
{
    return is_form(expr, "define");
}

static _Bool is_if(LispVal* expr)
{
    return is_form(expr, "if");
}

static _Bool is_lambda(LispVal* expr)
{
    return is_form(expr, "lambda");
}

static _Bool is_begin(LispVal* expr)
{
    return is_form(expr, "begin");
}

static _Bool is_application(LispVal* expr)
{
    return expr->tag == LCONS   // we actually can't know until we evaluate
                                // the head
        && good_list(expr->tail);
}

static _Bool is_primitive_proc(LispVal* expr)
{
    return expr->tag == LPRIM;
}

static _Bool is_compound_proc(LispVal* expr)
{
    return expr->tag == LLAM;
}

static _Bool is_nil(LispVal* expr)
{
    return expr->tag == LNIL;
}

static LispVal* lookup_variable_value(LispVal* expr)
{
    for (LispVal* e = env2; e->tag != LNIL; e = e->tail) {
        LispVal* nvp = e->head; // (name . value)
        if (sym_equal(expr->atom, nvp->head->atom)) {
            return nvp->tail;
        }
    }
    // what if it's not found
    return NULL;
}

static _Bool is_last_operand(LispVal* expr)
{
    return expr->tag == LCONS && expr->tail->tag == LNIL;
}

static LispVal* apply_primitive_proc(LispVal* fn, LispVal* args)
{
    // do we need to reverse the arguments?
    return fn->cfunc(args);
}

static _Bool is_truthy(LispVal* expr)
{
    return expr->tag != LBOOL || !expr->boolean;
}

static void print_reg(const char* regname, LispVal* reg)
{
    fprintf(stderr, "%s: ", regname);
    if (reg) print_lispval(stderr, reg);
    else fprintf(stderr, "(null)");
    fputc('\n', stderr);
}
static const char* routine_name(long long routine) {
    int routine_idx = routine >> 3LL;
    if (routine_idx <= 27 && routine_idx >= 0) {
        return ((const char*[]){
            "DONE",
            "EVAL_DISPATCH",
            "APPLY_DISPATCH",
            "EV_SELF_EVAL",
            "EV_VARIABLE",
            "EV_QUOTED",
            "EV_ASSIGNMENT",
            "EV_DEFINITION",
            "EV_DEFINITION_1",
            "EV_IF",
            "EV_IF_DECIDE",
            "EV_IF_ALTERNATE",
            "EV_IF_CONSEQUENT",
            "EV_LAMBDA",
            "EV_BEGIN",
            "EV_APPLICATION",
            "EVAL_ARGS",
            "EVAL_ARG_LOOP",
            "ACCUMULATE_ARG",
            "EVAL_LAST_ARG",
            "ACCUMULATE_LAST_ARG",
            "PRIMITIVE_APPLY",
            "COMPOUND_APPLY",
            "COMPOUND_APPLY_CONT",
            "EXTEND_ENV_LOOP",
            "EV_SEQUENCE",
            "EV_SEQUENCE_CONT",
            "EV_SEQUENCE_LAST_EXP"
        })[routine_idx];
    }
    return "(unknown)";
}
static void print_routine(const char* regname, long long routine)
{
    fprintf(stderr, "%s: %s\n", regname, routine_name(routine));
}

static void eval2_main_loop()
{
    for (;;) {
        // if we are debugging we could print out the state of the registers
        if (debug_eval2) {
            print_routine("pc", pc);
            print_reg("exp", expr2);
            print_reg("env", env2);
            print_reg("fun", fun2);
            print_reg("argl", argl2);
            print_routine("continue", continue2);
            print_reg("val", val2);
            print_reg("unev", unev2);
            // print_stack?
        }
        switch (pc) {
            case DONE:
                return;
            case EVAL_DISPATCH:
                if (is_self_evaluating(expr2)) {
                    pc = EV_SELF_EVAL;
                } else if (is_variable(expr2)) {
                    pc = EV_VARIABLE;
                } else if (is_quoted(expr2)) {
                    if (!good_list(expr2)) {
                        val2 = lisp_err("bad special form: quote");
                        pc = continue2;
                    } else if (list_length(expr2) != 2){
                        val2 = lisp_err("wrong number of args to special form: quote");
                        pc = continue2;
                    } else {
                        pc = EV_QUOTED;
                    }
                } else if (is_assignment(expr2)) {
                    pc = EV_ASSIGNMENT;
                } else if (is_definition(expr2)) {
                    if (!good_list(expr2) || list_length(expr2) != 3) {
                        val2 = lisp_err("bad special form: define");
                        pc = continue2;
                    } else {
                        pc = EV_DEFINITION;
                    }
                } else if (is_if(expr2)) {
                    if (!(good_list(expr2) && list_length(expr2) == 4)) {
                        val2 = lisp_err("incorrect syntax for if");
                        pc = continue2;
                    } else {
                        pc = EV_IF;
                    }
                } else if (is_lambda(expr2)) {
                    if (!good_list(expr2) || list_length(expr2) < 3) {
                        val2 = lisp_err("bad special form: lambda");
                        pc = continue2;
                    } else {
                        pc = EV_LAMBDA;
                    }
                } else if (is_begin(expr2)) {
                    if (!good_list(expr2) || list_length(expr2) >= 2) {
                        val2 = lisp_err("bad special form: begin");
                        pc = continue2;
                    }
                    pc = EV_BEGIN;
                } else if (is_application(expr2)) {
                    pc = EV_APPLICATION;
                } else {
                    pc = UNKNOWN_EXPR_ERROR;
                }
                break;
            case APPLY_DISPATCH:
                if (is_primitive_proc(fun2)) {
                    pc = PRIMITIVE_APPLY;
                } else if (is_compound_proc(fun2)) {
                    pc = COMPOUND_APPLY;
                } else {
                    pc = UNKNOWN_PROC_TYPE_ERROR;
                }
                break;
            case EV_SELF_EVAL:
                val2 = expr2;
                pc = continue2;
                break;
            case EV_VARIABLE:
                val2 = lookup_variable_value(expr2);
                if (val2 == NULL) {
                    fprintf(stderr, "var not found: %s\n", symtext(expr2->atom));
                    val2 = lisp_err("variable not found");
                }
                pc = continue2;
                break;
            case EV_QUOTED:
                // (quote quoted-expr)
                val2 = expr2->tail->head; // text-of-quotation
                pc = continue2;
                break;
            case EV_DEFINITION:
                // (define <variable> <expression>)
                unev2 = expr2->tail->head; // definition-variable
                save(unev2);
                expr2 = expr2->tail->tail->head; // definition-expression
                save(env2);
                save(continue2);
                continue2 = EV_DEFINITION_1;
                pc = EVAL_DISPATCH;
                break;
            case EV_DEFINITION_1:
                restore(&continue2);
                restore(&env2);
                restore(&unev2);
                // begin: define-variable!
                unev2 = lisp_cons(unev2, val2);
                env2 = lisp_cons(unev2, env2);
                // end: define-variable!
                pc = continue2;
                break;
            case EV_IF:
                // (if <test> <consequent> <alternate>)
                save(expr2);
                save(env2);
                save(continue2);
                continue2 = EV_IF_DECIDE;
                expr2 = expr2->tail->head; // if-predicate
                pc = EVAL_DISPATCH;
                break;
            case EV_IF_DECIDE:
                restore(&continue2);
                restore(&env2);
                restore(&expr2);
                if (is_truthy(val2)) {
                    pc = EV_IF_CONSEQUENT;
                    break;
                }
                pc = EV_IF_ALTERNATE; /* still assign this because of the
                                         debugger we will create */
                break;
            case EV_IF_ALTERNATE:
                expr2 = expr2->tail->tail->tail->head; // if-alternate
                pc = EVAL_DISPATCH;
                break;
            case EV_IF_CONSEQUENT:
                expr2 = expr2->tail->tail->head; // if-consequent
                pc = EVAL_DISPATCH;
                break;
            case EV_LAMBDA:
                // (lambda (params ...) body ...)
                unev2 = expr2->tail->head; // lambda-parameters
                expr2 = expr2->tail->tail; // lambda-body
                if (!good_list(unev2)) {
                    val2 = lisp_err("bad special form: params must be a list");
                } else {
                    val2 = lisp_lam(unev2, expr2, env2); // make-procedure
                }
                pc = continue2;
                break;
            case EV_APPLICATION:
                unev2 = expr2->tail; // operands
                expr2 = expr2->head; // operator
                save(continue2);
                save(env2);
                save(unev2);
                continue2 = EVAL_ARGS;
                pc = EVAL_DISPATCH;
                break;
            case EVAL_ARGS:
                restore(&unev2);
                restore(&env2);
                fun2 = val2;
                save(fun2);
                argl2 = lisp_nil(); // Want to make this not allocate...
                pc = EVAL_ARG_LOOP; /* would probably make sense to just
                                       fall-through */
                break;
            case EVAL_ARG_LOOP:
                save(argl2);
                expr2 = unev2->head;
                if (is_last_operand(unev2)) {
                    pc = EVAL_LAST_ARG;
                    break;
                }
                save(env2);
                save(unev2);
                continue2 = ACCUMULATE_ARG;
                pc = EVAL_DISPATCH;
                break;
            case ACCUMULATE_ARG:
                restore(&unev2);
                restore(&env2);
                restore(&argl2);
                argl2 = lisp_cons(val2, argl2);
                unev2 = unev2->tail;
                pc = EVAL_ARG_LOOP;
                break;
            case EVAL_LAST_ARG:
                continue2 = ACCUMULATE_LAST_ARG;
                pc = EVAL_DISPATCH;
                break;
            case ACCUMULATE_LAST_ARG:
                restore(&argl2);
                argl2 = lisp_cons(val2, argl2);
                restore(&fun2);
                pc = APPLY_DISPATCH;
                break;
            case PRIMITIVE_APPLY:
                val2 = apply_primitive_proc(fun2, argl2); // apply-primitive-proc
                restore(&continue2);
                pc = continue2;
                break;
            case COMPOUND_APPLY:
                unev2 = fun2->params; // procedure-parameters
                env2 = fun2->closure; // procedure-environment
                pc = EXTEND_ENV_LOOP;
                break;
            case EXTEND_ENV_LOOP:
                if (is_nil(unev2) || is_nil(argl2)) {
                    if (is_nil(unev2) && is_nil(argl2)) {
                        pc = COMPOUND_APPLY_CONT;
                    } else {
                        fprintf(stderr, "incorrect number of args\n");
                        pc = COMPOUND_APPLY_CONT; /* not sure of the effect
                                                     of not going here */
                    }
                    break;
                }
                // we can overwrite expr2 since it gets overwritten in
                // EV_SEQUENCE anyway
                expr2 = lisp_cons(unev2->head, argl2->head);
                env2 = lisp_cons(expr2, env2);
                unev2 = unev2->tail;
                argl2 = argl2->tail;
                pc = EXTEND_ENV_LOOP; // already is this but make explicit
                break;
            case COMPOUND_APPLY_CONT:
                unev2 = fun2->body; // procedure-body
                pc = EV_SEQUENCE;
                break;
            case EV_SEQUENCE:
                expr2 = unev2->head; // first-exp
                if (is_last_operand(unev2)) {
                    pc = EV_SEQUENCE_LAST_EXP;
                    break;
                }
                save(unev2);
                save(env2);
                continue2 = EV_SEQUENCE_CONT;
                pc = EVAL_DISPATCH;
                break;
            case EV_SEQUENCE_CONT:
                restore(&env2);
                restore(&unev2);
                unev2 = unev2->tail; // rest-exps
                pc = EV_SEQUENCE;
                break;
            case EV_SEQUENCE_LAST_EXP:
                restore(&continue2);
                pc = EVAL_DISPATCH;
                break;
            case EV_BEGIN:
                // (begin <action> ...)
                unev2 = expr2->tail; // begin-actions
                save(continue2);
                pc = EV_SEQUENCE;
                break;
            case UNKNOWN_EXPR_ERROR:
                fprintf(stderr, "error: unknown expression: ");
                print_lispval(stderr, expr2);
                fprintf(stderr, "\n");
                val2 = lisp_err("unknown-expression");
                pc = continue2; /* Need to let it restore the stack back to
                                   normal */
                break;
            case UNKNOWN_PROC_TYPE_ERROR:
                fprintf(stderr, "error: unknown proc type: ");
                print_lispval(stderr, expr2);
                fprintf(stderr, "\n");
                val2 = lisp_err("unknown-proc-type");
                pc = continue2;
                break;
            default:
                fprintf(stderr, "unknown operation\n");
                exit(EXIT_FAILURE);
                break;
        }
    }
}

LispVal* eval2(LispVal* expr)
{
    // set up the machine
    pc = EVAL_DISPATCH;
    continue2 = DONE;
    expr2 = expr;
    // TODO: set env2 = global environment?
    eval2_main_loop();
    // The result must now be in val2
    return val2;
}

void initialize_evaluator2()
{
    // set registers to nil
    env2 = lisp_nil();
    unev2 = env2;
    argl2 = env2;
    val2 = env2;
    expr2 = env2;
}

