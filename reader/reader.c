#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "reader.h"
#include "runtime.h"
#include "evaluator.h"
#include "eval2.h"

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

static const char* macro_name(int c)
{
    switch (c) {
        case '\'': return "quote";
        case '`': return "quasiquote";
        case ',': return "unquote";
        case COMMA_AT: return "unquote-splicing";
    }
    return NULL;
}

void push_lispval(LispVal* lv)
{
    const char* mn_inst = NULL;
    while (rs_ptr > reader_stack && (mn_inst = macro_name(rs_ptr[-1].tag))) {
        lv = lisp_cons(lisp_atom(sym(mn_inst)), lisp_cons(lv, lisp_nil()));
        pop_val();
        mn_inst = NULL;
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

lexswitch:
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
                    } else if (top->tag == '.') {
                        if (thelist->tag == LCONS && thelist->tail->tag == LNIL) {
                            // (...<rest> . <r1>)
                            thelist = thelist->head;
                        } else {
                            fprintf(stderr, "syntax error: . placement\n");
                            // don't actually do anything
                        }
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
            case '#':
            {
                // TODO !! perhaps we could deal with some
                // of this in the lexer instead...?
                lexval = yylex();
                switch (lexval) {
                    case '(':
                        // TODO: vector
                        break;
                    case '\\':
                        // TODO: character
                        lexval = yylex();
                        switch (lexval) {
                            case '.':
                            case '\'':
                            case '`':
                            case ',':
                            case ' ':
                            case '\t':
                            case '\f':
                            case '\v':
                            case '(':
                            case ')':
                                push_lispval(lisp_char(lexval));
                                break;
                            case COMMA_AT:
                                // TODO
                                break;
                            case VAR:
                                // Check for character names
                                // like #\space
                                if (sym_equal(sym("space"), yylval.id)) {
                                    push_lispval(lisp_char(' '));
                                } else if (sym_equal(sym("newline"), yylval.id)) {
                                    push_lispval(lisp_char('\n'));
                                } else if (sym_equal(sym("tab"), yylval.id)) {
                                    push_lispval(lisp_char('\t'));
                                } else if (strlen(symtext(yylval.id)) == 1) {
                                    push_lispval(lisp_char(
                                                symtext(yylval.id)[0]));
                                } else {
                                    // unknown character name
                                    fprintf(stderr,
                                            "unknown character name: %s\n",
                                            symtext(yylval.id));
                                    rs_ptr = reader_stack;
                                    return NULL;
                                }
                                break;
                            case 0:
                                return NULL; // EOF
                            case NUM:
                                // TODO: 0-9 should be fine
                            default:
                                // ignore?
                                continue;
                        }
                        break;
                    case VAR:
                        if (sym_equal(sym("t"), yylval.id)) {
                            push_lispval(lisp_bool(1));
                        } else if (sym_equal(sym("f"), yylval.id)) {
                            push_lispval(lisp_bool(0));
                        } else {
                            // combine the # with the symbol?
                            // error?
                            fprintf(stderr, "unknown reader macro #%s\n",
                                    symtext(yylval.id));
                            rs_ptr = reader_stack;
                            return NULL;
                        }
                        break;
                    case 0:
                        return NULL; // EOF
                    default:
                        push_lispval(lisp_atom(sym("#")));
                        goto lexswitch;
                }
                break;
            }
            case '.':
            case '\'':
            case '`':
            case ',':
            case COMMA_AT:
                push_val((tagged_stype){ .tag = lexval });
                continue;
            case ' ':
            case '\t':
            case '\f':
            case '\v':
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
    int use_eval2 = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-v") == 0) {
                verbose_gc = 1;
                debug_reader = 1;
            } else if (strcmp(argv[i], "-2") == 0) {
                use_eval2 = 1;
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
    if (use_eval2) {
        initialize_evaluator2();
    } else {
        initialize_evaluator();
    }
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
        LispVal* evaluated = (use_eval2) ? eval2(value) : eval(value);
        print_lispval(stdout, evaluated);
        printf("\n");

        if (debug_reader) {
            print_heap_state(); // Just to get a print of GC stats
        }
    }
    set_stack_high(&dummy);
}

