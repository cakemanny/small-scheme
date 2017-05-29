#ifndef __ASTLANG__AST_H__
#define __ASTLANG__AST_H__

#include "symbol.h"

#define DECLARE_STRUCT(x) struct x; typedef struct x x
DECLARE_STRUCT(DeclList );
DECLARE_STRUCT(Decl     );
DECLARE_STRUCT(AltList  );
DECLARE_STRUCT(Alt      );
DECLARE_STRUCT(NodeList );
DECLARE_STRUCT(Node     );

#define DECLARE_LIST(x) struct x##List { x* first; x##List* rest; }; \
    x##List* xcons##x(x* first, x##List* rest); \
    x##List* reverse##x##List(x##List* rest);
DECLARE_LIST(Decl);
DECLARE_LIST(Alt);
DECLARE_LIST(Node);

struct Decl {
    Symbol name;
    AltList* alts;

    // useful additional information for error reporting
    int lineno;
    int colno;
};
struct Alt {
    Symbol tag;
    NodeList* nodes;

    int lineno;
    int colno;
};
struct Node {
    Symbol name;
    enum TypeDisp {
        SCALAR_TYPE, LIST_TYPE
    } type_disp;
    Symbol typename;

    int lineno;
    int colno;
};

#define cons(hd,tail) _Generic((hd), \
        Decl*: xconsDecl, \
        Alt*: xconsAlt, \
        Node*: xconsNode)(hd, tail)
#define reverse_list(xs) _Generic((xs), \
        DeclList*: reverseDeclList, \
        AltList*: reverseAltList, \
        NodeList*: reverseNodeList)(xs)

Decl* mkdecl(Symbol name, AltList* alts, int lineno, int colno);

Alt* mkalt(Symbol tag, NodeList* nodes, int lineno, int colno);

Node* mknode_scalar_type(Symbol name, Symbol typename, int lineno, int colno);
Node* mknode_list_type(Symbol name, Symbol typename, int lineno, int colno);


// Just something useful for the lexer and parser to track line and column
// numbers
typedef struct SymAndPos {
    Symbol symbol;
    int lineno;
    int colno;
} SymAndPos;


#endif /* __ASTLANG__AST_H__  */
