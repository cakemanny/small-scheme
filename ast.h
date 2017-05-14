#ifndef __SS_AST_H__
#define __SS_AST_H__

struct Exp;
typedef struct Exp Exp;
struct Exp {
    enum ExpTag {
        APPEXP,
        LAMEXP,
        VAREXP
    } tag;
    union {
        struct { /* APPEXP */
            Exp* left;
            Exp* right;
        };
        struct { /* LAMEXP */
            char* param;
            Exp* body;
        };
        char* var; /* VAREXP */
    };
};

Exp* mkapp(Exp* left, Exp* right);
Exp* mklam(char* param, Exp* body);
Exp* mkvar(char* var);

#endif /* __SS_AST_H__ */
