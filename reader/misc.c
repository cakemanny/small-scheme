#include <stdio.h>
/*
 * We need some functions that cannot be optimized away by the compiler
 * so put them into this separate module so that it cannot see them where they
 * are used
 */

extern void** stack_ref_low;
extern void** stack_ref_high;


void set_stack_high(void** stack_high)
{
    stack_ref_high = stack_high;
    fprintf(stderr, "stack high = 0x%llx\n", (long long unsigned)stack_high);
}

void set_stack_low(void** stack_low)
{
    stack_ref_low = stack_low;
    fprintf(stderr, "stack low  = 0x%llx\n", (long long unsigned)stack_low);
}



