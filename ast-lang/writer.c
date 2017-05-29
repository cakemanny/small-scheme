#include "writer.h"
#include <stdlib.h>

extern DeclList* do_parse();

static int error_count = 0;

void sanity_check(DeclList* ast)
{
    for (DeclList* l = ast; l && l->rest; l = l->rest) {
        Decl* decl = l->first;
        // Check for duplicate declarations
        for (DeclList* l2 = l->rest; l2; l2 = l2->rest) {
            Decl* decl2 = l2->first;
            if (sym_equal(decl2->name, decl->name)) {
                error_count++;
                fprintf(stderr, "<stdin>:%d:%d: error: %s has already been "
                        "declared at %d:%d\n", decl2->lineno, decl2->colno,
                        symtext(decl2->name), decl->lineno, decl->colno);
            }
        }
        // Check that types exist?

        // Check that tags haven't been duplicated?
    }

}

void emit_header(FILE* out, DeclList* ast)
{
#define _s(x) symtext(x)
    // declare and produce typedefs so that definitions can be recursive
    for (DeclList* l = ast; l; l = l->rest) {
        Decl* decl = l->first;
        fprintf(out, "struct %s;\n", symtext(decl->name));
        fprintf(out, "typedef struct %s %s;\n",
                symtext(decl->name), symtext(decl->name));
    }
    fprintf(out, "\n");

    // Now produce the definitions of the structs

    for (DeclList* l = ast; l; l = l->rest) {
        Decl* decl = l->first;
        fprintf(out, "struct %s {\n", _s(decl->name));
        fprintf(out, "  enum %sTag {\n", _s(decl->name));
        for (AltList* as = decl->alts; as; as = as->rest) {
            fprintf(out, "    %s%s,\n", _s(decl->name), _s(as->first->tag));
        }
        fprintf(out, "  } tag;\n");

        fprintf(out, "  union {\n");
        for (AltList* as = decl->alts; as; as = as->rest) {
            Alt* alt = as->first;
            if (alt->nodes) {
                fprintf(out, "    struct /* %s */ {\n", _s(alt->tag));
                for (NodeList* ns = alt->nodes; ns; ns = ns->rest) {
                    Node* node = ns->first;
                    char is_ptr_type = sym_equal(sym("Symbol"), node->typename) ?
                        ' ' : '*' ;
                    if (node->type_disp == SCALAR_TYPE) {
                        fprintf(out, "      %s%c %s;\n", _s(node->typename),
                                is_ptr_type, _s(node->name));
                    } else { /* LIST_TYPE */
                        fprintf(out, "      %sList%c %s;\n", _s(node->typename),
                                is_ptr_type, _s(node->name));
                    }
                }
                fprintf(out, "    };\n");
            }
        }
        fprintf(out, "  };\n");

        fprintf(out, "};\n\n");
    }
#undef _s
}

int main(int argc, char** argv)
{
    DeclList* ast = do_parse();
    if (!ast) {
        fprintf(stderr, "Parse failed\n");
        exit(EXIT_FAILURE);
    }

    // Want to perform some sanity checks on the input before performing
    // output as c and losing track of the original input
    sanity_check(ast);
    if (error_count > 0) {
        exit(EXIT_FAILURE);
    }

    emit_header(stdout, ast);

}

