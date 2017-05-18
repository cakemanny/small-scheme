%{
#include <stdio.h>
#include "ast.h"
#include "runtime.h"

/* the function the lexer exports */
int yylex();
void yyerror(const char*);

static Exp* tree = NULL;

%}

%union {
    /* Tree */
    Exp*    exp;
    /* Lexer terminals */
    Symbol   id;
}

%token LAMBDA VAR NUM
%type <id> VAR NUM
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
  | NUM
    {
        $$ = mknum($1);
    }
  ;

%%


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

        LispVal* result = eval(tree);
        print_val(stdout, result);
        fprintf(stdout, "\n");
    }
}

