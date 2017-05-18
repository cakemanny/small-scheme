#include "ast.h"
#include <stdlib.h>
#include <stdio.h>

Exp* mkapp(Exp* left, Exp* right)
{
    Exp* result = malloc(sizeof *result);
    result->tag = APPEXP;
    result->left = left;
    result->right = right;
    return result;
}

Exp* mklam(Symbol param, Exp* body)
{
    Exp* result = malloc(sizeof *result);
    result->tag = LAMEXP;
    result->param = param;
    result->body = body;
    return result;
}

Exp* mkvar(Symbol var)
{
    Exp* result = malloc(sizeof *result);
    result->tag = VAREXP;
    result->var = var;
    return result;
}

Exp* mknum(Symbol num)
{
    Exp* result = malloc(sizeof *result);
    result->tag = NUMEXP;
    result->num = num;
    return result;
}

void print_exp(FILE* out, Exp* exp)
{
    switch (exp->tag)
    {
    case APPEXP:
        fputc('(', out);
        print_exp(out, exp->left);
        fputc(' ', out);
        print_exp(out, exp->right);
        fputc(')', out);
        break;
    case LAMEXP:
        fprintf(out, "(lambda (%s) ", symtext(exp->param));
        print_exp(out, exp->body);
        fputc(')', out);
        break;
    case VAREXP:
        fprintf(out, "%s", symtext(exp->var));
        break;
    case NUMEXP:
        fprintf(out, "%s", symtext(exp->num));
        break;
    }
}


