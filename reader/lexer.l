%{
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "symbol.h"
#include "tokens.h"

int yywrap();

%}

INITIAL [*/^!$%&|:<=>?^_~]|[[:alpha:]]
SUBSEQ  {INITIAL}|[-+.@0-9]
VAR     [-+]|"..."|{INITIAL}{SUBSEQ}*
NUM     [-+]?(0|[1-9][0-9]*)

%%

;.*$            /* swallow comments */

{VAR}           { yylval.id = sym(yytext); return VAR; }

{NUM}           { yylval.id = sym(yytext); return NUM; }

,@              { return COMMA_AT; }
[()#',.@`\\]    { return yytext[0]; }

[ \t\f\v]       { return yytext[0]; } /* ignored by the reader in most cases */
[\n]            { yylineno++; }

.               { yylval.err_char = yytext[0]; return ERROR; }

%%

int yywrap() { return 1; }

