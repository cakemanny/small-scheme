#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "reader.h"
#include "runtime.h"
#include "evaluator.h"

int debug_lexer = 0;
int debug_reader = 0;

extern int yylex();
extern FILE* yyin;

union yystype yylval;


tagged_stype* reader_stack;
tagged_stype* rs_ptr;

void push_val(tagged_stype val)
{
    *rs_ptr++ = val;
}

tagged_stype* pop_val()
{
    if (rs_ptr > reader_stack) {
        return --rs_ptr;
    }
    return NULL;
}

void push_lispval(LispVal* lv)
{
    while (rs_ptr > reader_stack && rs_ptr[-1].tag == '\'') {
        lv = lisp_cons(lisp_atom(sym("quote")), lisp_cons(lv, lisp_nil()));
        pop_val();
    }
    push_val((tagged_stype){
        .tag = LISPVAL,
        .sval.value = lv
    });
}


static LispVal* reader_read()
{
    int num_parens = 0;
    int lexval;
    while ((lexval = yylex()) != 0) {
        if (debug_lexer) {
            fprintf(stderr, "LEXVAL = \"%c\" 0x%x, %d\n", lexval, lexval, lexval);
        }

        switch (lexval) {
            case ERROR:
            {
                if (debug_lexer) {
                    fprintf(stderr, "ERROR(%c)\n", yylval.err_char);
                }
                // reset stack
                rs_ptr = reader_stack;
                return NULL;
            }
            case NUM:
            {
                push_lispval(lisp_num(atoi(symtext(yylval.id))));
                break;
            }
            case VAR:
            {
                push_lispval(lisp_atom(yylval.id));
                break;
            }
            case '(':
            {
                push_val((tagged_stype){ .tag = '(' });
                num_parens++;
                break;
            }
            case ')':
            {
                if (num_parens == 0) {
                    fprintf(stderr, "syntax error\n");
                    break;
                }

                //mark_safepoint(); // communicate with the collector
                // collapse stack into val
                LispVal* thelist = lisp_nil();
                tagged_stype* top;
                while ((top = pop_val())) {
                    if (top->tag == '(')
                        break;
                    // build the list up from it's tail
                    if (top->tag == LISPVAL) {
                        thelist = lisp_cons(top->sval.value, thelist);
                    } else {
                        // TODO: what happens here!
                    }
                }
                // push the constructed list back on
                push_lispval(thelist);
                num_parens--;

                //mark_safepoint(); // communicate with the collector
                break;
            }
            case '\'':
                push_val((tagged_stype){ .tag = '\'' });
                continue;
            default:
                fprintf(stderr, "%c\n", lexval);
                break;
        }
        if (num_parens == 0) {
            // return!
            tagged_stype* top = pop_val();
            if (top && top->tag == LISPVAL) {
                return top->sval.value;
            } else {
                fprintf(stderr, "bad something...");
                return NULL;
            }
        }
    }
    return NULL; // End of file
}

void set_stack_high(void** stack_high);
void set_stack_low(void** stack_low);
extern int verbose_gc;

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-v") == 0) {
                verbose_gc = 1;
                debug_reader = 1;
            } else {
                fprintf(stderr, "unknown flag: -%c", argv[i][1]);
            }
        } else {
            yyin = fopen(argv[i], "r");
            if (!yyin) {
                perror(argv[i]);
                exit(EXIT_FAILURE);
            }
        }
    }
    initialize_heap(512 * 1024);
    initialize_evaluator();
    // initialise the reader stack
    reader_stack = calloc(1024, sizeof *reader_stack);
    if (!reader_stack) { perror("out of memory"); abort(); }
    rs_ptr = reader_stack;

    /*
     * A neat, asm-free way I came up with of safely grabbing some stack
     * bounds.
     * set_stack_high and set_stack_low are in a separate module so that
     * the optimizer can't (assume no LTO) get rid of the stack assignment
     */
    void* dummy = 0; // use ptr word to get word alignment
    set_stack_high(&dummy);

    for (;;) {
        LispVal* value = reader_read();
        if (!value && feof(yyin))
            break;
        if (!value)
            continue;
        //print_lispval(stdout, value);
        //printf("\n");
        LispVal* evaluated = eval(value);
        print_lispval(stdout, evaluated);
        printf("\n");

        if (debug_reader) {
            print_heap_state(); // Just to get a print of GC stats
        }
    }
    set_stack_high(&dummy);
}

