%{
#include <stdio.h>
#include "ast.h"

/* the function the lexer exports */
int yylex();
void yyerror(const char*);

static Exp* tree = NULL;

%}

%union {
    /* Tree */
    Exp*    exp;
    /* Lexer terminals */
    char*   id;
}

%token LAMBDA VAR
%type <id> VAR
%type <exp> exp prog

%%

prog: exp           { $$ = tree = $1; }

exp:
    '(' exp exp ')'
    {
        $$ = mkapp($2, $3);
    }
  | '(' LAMBDA '(' VAR ')' exp ')'
    {
        $$ = mklam($4, $6);
    }
  | VAR
    {
        $$ = mkvar($1);
    }
  ;

%%

static void print_exp(FILE* out, Exp* exp);

void yyerror(const char* msg)
{
    fprintf(stderr, "%s\n", msg);
}

int yywrap() { return 1; }

int main()
{
    yyparse();
    if (tree) {
        print_exp(stdout, tree);
        fprintf(stdout, "\n");
    }
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
        fprintf(out, "(lambda (%s) ", exp->param);
        print_exp(out, exp->body);
        fputc(')', out);
        break;
    case VAREXP:
        fprintf(out, "%s", exp->var);
        break;
    }
}

Exp* mkapp(Exp* left, Exp* right)
{
    Exp* result = malloc(sizeof *result);
    result->tag = APPEXP;
    result->left = left;
    result->right = right;
    return result;
}

Exp* mklam(char* param, Exp* body)
{
    Exp* result = malloc(sizeof *result);
    result->tag = LAMEXP;
    result->param = param;
    result->body = body;
    return result;
}

Exp* mkvar(char* var)
{
    Exp* result = malloc(sizeof *result);
    result->tag = VAREXP;
    result->var = var;
    return result;
}

