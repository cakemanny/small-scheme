%{
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "symbol.h"
#include "ast.h"
#include "grammar.tab.h"

%}

INITIAL [*/^!$%&|:<=>?^_~]|[[:alpha:]]
SUBSEQ  {INITIAL}|[-+.@0-9]
VAR     [-+]|"..."|{INITIAL}{SUBSEQ}*
NUM     [-+]?(0|[1-9][0-9]*)

%%

;.*$        /* swallow comments */

lambda      { return LAMBDA; }

{VAR}       { yylval.id = sym(yytext); return VAR; }

{NUM}       { yylval.id = sym(yytext); return NUM; }

[()#',.@]   { return yytext[0]; }

[ \t\f\v]   /* swallow */
[\n]        { yylineno++; }

%%


