#ifndef __SS__SYMBOL_H__
#define __SS__SYMBOL_H__

typedef struct Symbol {
    const char* name;
} Symbol;

Symbol sym(const char* name);

static inline const char* symtext(const Symbol symbol)
{
    return symbol.name;
}

static inline int sym_equal(const Symbol left, const Symbol right)
{
    return left.name == right.name;
}

#endif /* __SS__SYMBOL_H__ */
