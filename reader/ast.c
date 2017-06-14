
#include "ast.h"
#include "runtime.h"

static const char* tag_names[9] = {
    "LATOM", "LNUM", "LCONS", "LNIL", "LLAM", "LPRIM", "LBOOL", "LERROR", "LCHAR"
};

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

LispVal* lisp_bool(_Bool boolean)
{
    LispVal* result = lispval(LBOOL);
    result->boolean = boolean;
    return result;
}

LispVal* lisp_err(const char* error_msg)
{
    LispVal* result = lispval(LERROR);
    result->error_msg = error_msg;
    return result;
}

LispVal* lisp_char(int character)
{
    LispVal* result = lispval(LCHAR);
    result->character = character;
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
            for (LispVal* e = value->body; e->tag == LCONS; e = e->tail) {
                fputc(' ', out);
                print_lispval(out, e->head);
            }
            fputc(')', out);
            break;
        case LPRIM:
            fprintf(out, "<primitive>");
            break;
        case LBOOL:
            fputs((value->boolean) ? "#t" : "#f", out);
            break;
        case LERROR:
            fprintf(out, "error: %s", value->error_msg);
            break;
        case LCHAR:
            if (value->character == ' ') {
                fprintf(out, "#\\space");
            } else if (value->character == '\n') {
                fprintf(out, "#\\newline");
            } else if (value->character == '\t') {
                fprintf(out, "#\\tab");
            } else {
                fprintf(out, "#\\%c", value->character);
            }
            break;
    }
}

