
#include "ast.h"
#include "runtime.h"

static const char* tag_names[6] =
    { "LATOM", "LNUM", "LCONS", "LNIL", "LPRIM", "LLAM" };

const char* lv_tagname(LispVal* value)
{
    return tag_names[value->tag];
}

static LispVal* lispval(enum LispTag tag)
{
    LispVal* result = lisp_alloc(sizeof *result);
    result->tag = tag;
    return result;
}

LispVal* lisp_atom(Symbol atom)
{
    LispVal* result = lispval(LATOM);
    result->atom = atom;
    return result;
}

LispVal* lisp_num(int number)
{
    LispVal* result = lispval(LNUM);
    result->number = number;
    return result;
}

LispVal* lisp_cons(LispVal* head, LispVal* tail)
{
    LispVal* result = lispval(LCONS);
    result->head = head;
    result->tail = tail;
    return result;
}

LispVal* lisp_nil()
{
    return lispval(LNIL);
}

LispVal* lisp_lam(LispVal* params, LispVal* body, LispVal* closure)
{
    LispVal* result = lispval(LLAM);
    result->params = params;
    result->body = body;
    result->closure = closure;
    return result;
}

LispVal* lisp_prim(primfunc cfunc)
{
    LispVal* result = lispval(LPRIM);
    result->cfunc = cfunc;
    return result;
}


void print_lispval(FILE* out, LispVal* value)
{
    switch (value->tag) {
        case LATOM:
            fprintf(out, "%s", symtext(value->atom));
            break;
        case LNUM:
            fprintf(out, "%d", value->number);
            break;
        case LCONS:
            fputc('(', out);
            print_lispval(out, value->head);
            while (value->tail->tag == LCONS) {
                fputc(' ', out);
                value = value->tail;
                print_lispval(out, value->head);
            }
            if (value->tail->tag != LNIL) {
                fputs(" . ", out);
                print_lispval(out, value->tail);
            }
            fputc(')', out);
            break;
        case LNIL:
            fprintf(out, "()");
            break;
        case LLAM:
            fprintf(out, "(lambda ");
            print_lispval(out, value->params);
            fputc(' ', out);
            print_lispval(out, value->body);
            fputc(')', out);
            break;
        case LPRIM:
            fprintf(out, "<primitive>");
            break;
    }
}

