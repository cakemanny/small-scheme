%{
#include "ast.h"
#include <stdio.h>
#include <assert.h>

extern int yylex();
static void yyerror(const char* msg);

/* Line number from flex lexer */
extern int yylineno;
/* Input file used by flex lexer */
extern FILE* yyin;

static DeclList* tree;


%}

%union {
  /* Parts of the tree */
    DeclList*   decls;
    Decl*       decl;
    AltList*    alts;
    Alt*        alt;
    NodeList*   nodes;
    Node*       node;

/* terminals  */
    SymAndPos   id;
    char        bad_char;
}

%token NODE TYPE ERROR

%type <decls> declarations
%type <decl> declaration
%type <alts> alternatives
%type <alt> alternative
%type <nodes> nodes
%type <node> node
%type <id> TYPE NODE
%type <bad_char> ERROR


%%

program:
    declarations                    { tree = $1; }
  ;
declarations:
    declarations declaration        { $$ = cons($2, $1); }
  | declaration                     { $$ = cons($1, NULL); }
  ;
declaration:
    TYPE '=' alternatives ';'
        { $$ = mkdecl($1.symbol, reverse_list($3), $1.lineno, $1.colno); }
  ;
alternatives:
    alternatives '|' alternative    { $$ = cons($3, $1); }
  | alternative                     { $$ = cons($1, NULL); }
  ;
alternative:
    TYPE
        { $$ = mkalt($1.symbol, NULL, $1.lineno, $1.colno); }
  | TYPE nodes
        { $$ = mkalt($1.symbol, reverse_list($2), $1.lineno, $1.colno); }
  ;
nodes:
    nodes node                      { $$ = cons($2, $1); }
  | node                            { $$ = cons($1, NULL); }
  ;
node:
    NODE ':' TYPE
        { $$ = mknode_scalar_type($1.symbol, $3.symbol, $1.lineno, $1.colno); }
  | NODE ':' '[' TYPE ']'
        { $$ = mknode_list_type($1.symbol, $4.symbol, $1.lineno, $1.colno); }
  ;

%%

static void yyerror(const char* msg)
{
    fprintf(stderr, "error:<0>:%d: %s\n", yylineno, msg);
}

int yywrap() { return 1; }

void print_lex(int lexeme)
{
    if (lexeme < 255) {
        fprintf(stderr, "%c\n", lexeme);
    } else {
        switch (lexeme) {
            case NODE:
                fprintf(stderr, "NODE(%s)\n", symtext(yylval.id.symbol));
                break;
            case TYPE:
                fprintf(stderr, "TYPE(%s)\n", symtext(yylval.id.symbol));
                break;
            case ERROR:
                fprintf(stderr, "ERROR(%c)\n", yylval.bad_char);
                break;
            default:
                assert(0 && "missing case");
                break;
        }
    }
}

DeclList* do_parse()
{
    yyparse();
    return (tree) ? reverse_list(tree) : NULL;
}

