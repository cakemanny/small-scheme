#ifndef __RUNTIME__AST_H__
#define __RUNTIME__AST_H__

#include <stdlib.h>

void* lisp_alloc(size_t size);

void initialize_heap(size_t initial_heap_size);

/*
 * Mark a line in the sand for the collector that things allocated after this
 * point may not be visible to it
 */
void mark_safepoint();

// Just to get a print of GC stats
void print_heap_state();

#endif /* __RUNTIME__AST_H__ */
