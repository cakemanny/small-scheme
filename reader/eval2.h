#ifndef __READER__EVAL2_H__
#define __READER__EVAL2_H__

#include "ast.h"

void initialize_evaluator2();

LispVal* eval2(LispVal* expr);

#endif /* __READER__EVAL2_H__ */
