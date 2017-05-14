%{
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ast.h"
#include "grammar.tab.h"

%}

INITIAL [-+*/^!$%&|:<=>?^_~]|[[:alpha:]]
SUBSEQ  {INITIAL}|[0-9.@]
VAR     {INITIAL}{SUBSEQ}*

%%

;.*$        /* swallow comments */

lambda      { return LAMBDA; }

{VAR}       { yylval.id = strdup(yytext); return VAR; }

[()]        { return yytext[0]; }

[ \t\f\v]   /* swallow */
[\n]        { yylineno++; }

%%


