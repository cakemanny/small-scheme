#ifndef __ASTLANG__WRITER_H__
#define __ASTLANG__WRITER_H__

#include <stdio.h> // FILE*
#include "ast.h"

void emit_header(FILE* out, DeclList* ast);

#endif /* __ASTLANG__WRITER_H__ */
