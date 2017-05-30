#include <stdio.h>
#include <stdlib.h>
#include "reader.h"
#include "runtime.h"
#include "evaluator.h"

int debug_lexer = 0;

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
                push_val((tagged_stype){
                    .tag = LISPVAL,
                    .sval.value = lisp_num(atoi(symtext(yylval.id)))
                });
                break;
            }
            case VAR:
            {
                push_val((tagged_stype){
                    .tag = LISPVAL,
                    .sval.value = lisp_atom(yylval.id)
                });
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
                push_val((tagged_stype){
                    .tag = LISPVAL, .sval.value = thelist
                });
                num_parens--;

                //mark_safepoint(); // communicate with the collector
                break;
            }
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

int main(int argc, char** argv)
{
    if (argc > 1) {
        yyin = fopen(argv[1], "r");
        if (!yyin) {
            perror(argv[1]);
            exit(EXIT_FAILURE);
        }
    }
    initialize_heap(4 * 1024);
    initialize_evaluator();
    // initialise the reader stack
    reader_stack = calloc(1024, sizeof *reader_stack);
    if (!reader_stack) {
        perror("out of memory");
        abort();
    }
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
        //print_lispval(stdout, value);
        //printf("\n");
        LispVal* evaluated = eval(value);
        print_lispval(stdout, evaluated);
        printf("\n");

        print_heap_state(); // Just to get a print of GC stats
    }
    set_stack_high(&dummy);
}

