#include "ast.h"
#include <stdlib.h>
#include <stdio.h>

static void* xmalloc(size_t size)
{
    void* result = malloc(size);
    if (!result) {
        perror("out of memory");
        abort();
    }
    return result;
}

Decl* mkdecl(Symbol name, AltList* alts, int lineno, int colno)
{
    Decl* result = malloc(sizeof *result);
    result->name = name;
    result->alts = alts;
    result->lineno = lineno;
    result->colno = colno;
    return result;
}

Alt* mkalt(Symbol tag, NodeList* nodes, int lineno, int colno)
{
    Alt* result = malloc(sizeof *result);
    result->tag = tag;
    result->nodes = nodes;
    result->lineno = lineno;
    result->colno = colno;
    return result;
}

Node* mknode_scalar_type(Symbol name, Symbol typename, int lineno, int colno)
{
    Node* result = malloc(sizeof *result);
    result->name = name;
    result->type_disp = SCALAR_TYPE;
    result->typename = typename;
    result->lineno = lineno;
    result->colno = colno;
    return result;
}

Node* mknode_list_type(Symbol name, Symbol typename, int lineno, int colno)
{
    Node* result = malloc(sizeof *result);
    result->name = name;
    result->type_disp = LIST_TYPE;
    result->typename = typename;
    result->lineno = lineno;
    result->colno = colno;
    return result;
}

#define REVERSE_LIST_IMPL(T, list)              \
{                                               \
    T* tmp;                                     \
    T* newhead = NULL;                          \
    while (list) { /* three pointer shuffle! */ \
        tmp         = list->rest;               \
        list->rest  = newhead;                  \
        newhead     = list;                     \
        list        = tmp;                      \
    }                                           \
    return newhead;                             \
}

#define REVERSE_LIST_DEF(T)                     \
T##List* reverse##T##List(T##List* list)        \
{                                               \
    REVERSE_LIST_IMPL(T##List, list);           \
}

#define XCONS_DEF(T)                                \
T##List* xcons##T(T* head, T##List* tail)           \
{                                                   \
    T##List* result = xmalloc(sizeof *result);      \
    result->first = head;                           \
    result->rest = tail;                            \
    return result;                                  \
}

#define DEF_LIST(T) REVERSE_LIST_DEF(T); XCONS_DEF(T)
DEF_LIST(Decl);
DEF_LIST(Alt);
DEF_LIST(Node);


