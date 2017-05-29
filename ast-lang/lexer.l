%{
#include <stdio.h>
#include "symbol.h"
#include "ast.h"
#include "grammar.tab.h"

static int colno = 0;

%}

TYPE    [A-Z][A-Za-z0-9_]*
NODE    [a-z][A-Za-z0-9_]*

%%

 /* c-style comments */
"/*"            {
                    colno += 2;
                    /* swallow comments */
                    int c;
                    while ((c = input()) != 0) {
                        ++colno;
                        if (c == '\n') {
                            ++yylineno;
                            colno = 0;
                        } else if (c == '*') {
                            c = input();
                            if (c == '/') {
                                break;
                            } else {
                                unput(c);
                            }
                        } else if (c < 0) {
                            return ERROR;
                        }
                    }
                }

{TYPE}          {
                    yylval.id.symbol = sym(yytext);
                    yylval.id.lineno = yylineno;
                    yylval.id.colno = colno + 1;
                    colno += strlen(yytext);
                    return TYPE;
                }

{NODE}          {
                    yylval.id.symbol = sym(yytext);
                    yylval.id.lineno = yylineno;
                    yylval.id.colno = colno + 1;
                    colno += strlen(yytext);
                    return NODE;
                }

[=|;:]          |
"["             |
"]"             { ++colno; return yytext[0]; }

[ \t\f\v\r]     /* swallow whitespace */ { ++colno; }
[\n]            {
                    ++yylineno;
                    colno = 0;
                }

.               {
                    ++colno;
                    // TODO: record the column number somewhere
                    yylval.bad_char = yytext[0];
                    return ERROR;
                }

%%



