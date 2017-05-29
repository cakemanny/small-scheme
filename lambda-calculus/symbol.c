#include "symbol.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static Symbol* symbol_table = NULL;
static int symbol_table_size = 0;
static int sym_table_count = 0;

Symbol sym(const char* name)
{
    if (sym_table_count >= symbol_table_size) {
        int new_table_size = symbol_table_size ? 2 * symbol_table_size : 1024;
        Symbol* new_table =
            malloc(new_table_size * sizeof *symbol_table);
        if (!new_table) {
            perror("out of memory");
            abort();
        }
        if (symbol_table_size) {
            memcpy(new_table, symbol_table, symbol_table_size * sizeof *symbol_table);
        }
        free(symbol_table);
        symbol_table = new_table;
        symbol_table_size = new_table_size;
    }
    for (int i = 0; i < sym_table_count; i++) {
        if (name == symbol_table[i].name)
            return symbol_table[i];
        if (strcmp(name, symbol_table[i].name) == 0) {
            return symbol_table[i];
        }
    }
    return symbol_table[sym_table_count++] = (Symbol){
        .name = strdup(name)
    };
}


