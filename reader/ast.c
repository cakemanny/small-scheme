
#include "ast.h"
#include "runtime.h"

static const char* tag_names[6] =
    { "LATOM", "LNUM", "LCONS", "LNIL", "LPRIM", "LLAM" };

const char* lv_tagname(LispVal* value)
{
    return tag_names[value->tag];
}

LispVal* lisp_atom(Symbol atom)
{
    LispVal* result = lisp_alloc(sizeof *result);
    result->tag = LATOM;
    result->atom = atom;
    return result;
}

LispVal* lisp_num(int number)
{
    LispVal* result = lisp_alloc(sizeof *result);
    result->tag = LNUM;
    result->number = number;
    return result;
}

LispVal* lisp_cons(LispVal* head, LispVal* tail)
{
    LispVal* result = lisp_alloc(sizeof *result);
    result->tag = LCONS;
    result->head = head;
    result->tail = tail;
    return result;
}

LispVal* lisp_nil()
{
    LispVal* result = lisp_alloc(sizeof *result);
    result->tag = LNIL;
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
    }
}

