#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ast.h"
#include "tokens.h"
#include "reader.h"

int verbose_gc = 1;

static void* heaps[2];
static int heap_size;
static int heap_idx = 0;
static void* free_ptr;
static void* safe_line;

static void collect();

#define ALIGNPTR(x) do { x = (void*)( ((size_t)(x + 7LL)) & -8LL ); } while(0)

void* lisp_alloc(size_t size)
{
    if (free_ptr + size < heaps[heap_idx] + heap_size) {
        void* result = free_ptr;
        free_ptr += size;
        ALIGNPTR(free_ptr);
        return result;
    } else {
        collect();
        if (free_ptr + size < heaps[heap_idx] + heap_size) {
            return lisp_alloc(size);
        }
        fprintf(stderr, "gc: out of memory!\n");
        exit(EXIT_FAILURE);
    }
}

void mark_safepoint()
{
    safe_line = free_ptr;
    float pct_full = (float)(free_ptr - heaps[heap_idx]) / ((float)(heap_size));
    if (verbose_gc) {
        fprintf(stderr, "%.2f heap used\n", pct_full);
    }
    if (pct_full > 0.7f) {
        collect();
    }
}

typedef struct Trail {
    LispVal** val_ptr;
    struct Trail* next;
} Trail;
// Need to fit our trail node in a LispVal space...
_Static_assert(sizeof(Trail) <= sizeof(LispVal),
        "Trail must fit in space of a freed LispVal");

void collect()
{
    // Need:
    // 1. Stack Roots
    // 2. Environment root
    // 3. Reader_stack
    fprintf(stderr, "performing collection\n");

    int next_heap_idx = (heap_idx + 1) & 1;
    void* next_free_ptr = heaps[next_heap_idx];

    // copy anything that is being read by the reader
    // The forms being read by the reader should just be directed-acyclic-trees
    // so hopefully nothing too complicated here
    tagged_stype* rs = reader_stack;
    tagged_stype* rsend = rs_ptr;

    int lv_size = sizeof(LispVal);
    for (tagged_stype* p = rs; p < rsend; p++) {
        if (p->tag == LISPVAL) {
            LispVal** current = &p->sval.value;
            Trail* trail_start = NULL;
            for (;;) {
                // 1. Copy value
                // 2. Copy the things it points to
                memcpy(next_free_ptr, *current, lv_size);
                void* spare_space = *current; /* Save the space we've just freed
                                                for use in this algorithm */
                *current = next_free_ptr;
                next_free_ptr += lv_size;
                ALIGNPTR(free_ptr);

                if ((*current)->tag == LCONS) {
                    if (verbose_gc)
                        fprintf(stderr, "it's a cons, follow head, save tail\n");
                    // -- save a pointer to the tail somewhere
                    // we can use the space we just made by copying current
                    // to store a linked list containing the tail pointers
                    // that we need to come back to
                    Trail* trail_head = spare_space;
                    trail_head->val_ptr = &(*current)->tail;
                    trail_head->next = trail_start;
                    trail_start = trail_head;

                    // stop-copy the head
                    current = &(*current)->head;
                    // Allow us to loop! (would be a recurse in a functional
                    // language)
                } else if (trail_start) {
                    if (verbose_gc)
                        fprintf(stderr, "it's a %s, clean up our saved tails\n",
                            lv_tagname(*current));
                    // Clean up the trail mess we've made
                    // Assume we can do this in any order - just take the head
                    current = trail_start->val_ptr;
                    trail_start = trail_start->next;
                    // recurse! (or loop as it's known in c :P )
                } else {
                    if (verbose_gc)
                        fprintf(stderr, "it's a %s, nothing left on this trail\n",
                            lv_tagname(*current));
                    // No mess and we are not a CONS! DONE! (for this item)
                    break;
                }
            }
            // Do we do anything here?
        }
    }
    heap_idx = next_heap_idx;
    free_ptr = next_free_ptr;
    if (verbose_gc) {
        fprintf(stderr, "gc: collection finished\n");
        float pct_full = (float)(free_ptr - heaps[heap_idx]) / ((float)(heap_size));
        fprintf(stderr, "%.2f heap used\n", pct_full);
    }
}

void initialize_heap(size_t initial_heap_size)
{
    heap_size = initial_heap_size;
    for (int i = 0; i < 2; i++) {
        heaps[i] = malloc(initial_heap_size);
        if (!heaps[i]) { perror("out of memory"); abort(); }
    }
    free_ptr = heaps[0];
}

