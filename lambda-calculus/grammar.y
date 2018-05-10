%{
#include <stdio.h>
#include "ast.h"
#include "runtime.h"
#include <unistd.h>

/* the function the lexer exports */
int yylex();
static void yyerror(const char*);

static void evaluate_last_expression(Exp* tree);

int debug_parser = 0;

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

prog: exp           { evaluate_last_expression($1); }
    | prog exp      { evaluate_last_expression($2); }
;

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


static void yyerror(const char* msg)
{
    fprintf(stderr, "%s\n", msg);
}

int yywrap() { return 1; }

static void evaluate_last_expression(Exp* tree)
{
    if (tree) {
        if (debug_parser) {
            print_exp(stdout, tree);
            fprintf(stdout, "\n");
        }

        LispVal* result = eval(tree);
        print_val(stdout, result);
        fprintf(stdout, "\n");

        if (isatty(0)) {
            printf("λ=>");
        }
    }
}

int main()
{
    while (!feof(stdin)) {
        if (isatty(0)) {
            printf("λ=>");
        }
        yyparse();
    }
}

