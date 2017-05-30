#ifndef __READER__EVALUATOR_H__
#define __READER__EVALUATOR_H__

#include "ast.h"

void initialize_evaluator();

LispVal* eval(LispVal* expr);

// The environment, so that the GC can take a look
extern LispVal* env;

#endif /* __READER__EVALUATOR_H__ */
