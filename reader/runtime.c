#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ast.h"
#include "tokens.h"
#include "reader.h"
#include "evaluator.h"

int verbose_gc = 0;

static void* heaps[2];
static int heap_size;
static int heap_idx = 0;
static void* free_ptr;
static void* safe_line;

static struct GCStats {
    long long num_collections;
    long long total_bytes_allocated;
    long long total_bytes_retained;
} gc_stats;

static void collect();

#define ALIGNPTR(x) do { x = (void*)( ((size_t)(x + 7LL)) & -8LL ); } while(0)

void** stack_ref_low; // void** - cast stack as array of ptrs
void** stack_ref_high;
void set_stack_high(void** stack_high);
void set_stack_low(void** stack_low);

void* lisp_alloc(size_t size)
{
    if (free_ptr + size < heaps[heap_idx] + heap_size) {
        void* result = free_ptr;
        memset(result, 0, size); /* want nice clean data. But only memset when
                                    mutator about to write anyway */
        free_ptr += size;
        ALIGNPTR(free_ptr);

        gc_stats.total_bytes_allocated += size;
        return result;
    } else {
        void* dummy = 0;
        set_stack_low(&dummy); // See comment above set_stack_high in reader.c
        collect();
        set_stack_low(&dummy);

        if (free_ptr + size < heaps[heap_idx] + heap_size) {
            return lisp_alloc(size);
        }
        fprintf(stderr, "gc: out of memory!\n");
        exit(EXIT_FAILURE);
    }
}

static float pct_full()
{
    return (float)(free_ptr - heaps[heap_idx]) / ((float)(heap_size));
}

void print_heap_state()
{
    fprintf(stderr, "- heap used: %.2f\n", pct_full());
    fprintf(stderr, "- num collections: %lld\n", gc_stats.num_collections);
    double avg_bytes_retained =
        ((double)gc_stats.total_bytes_retained)
            / ((double)gc_stats.num_collections);
    fprintf(stderr, "- avg heap retained (bytes): %lf\n", avg_bytes_retained);
    fprintf(stderr, "- avg heap retained (proportion): %f\n",
            avg_bytes_retained / ((double)heap_size));
    fprintf(stderr, "- total bytes allocated: %lld\n",
            gc_stats.total_bytes_allocated);
}

void mark_safepoint()
{
    safe_line = free_ptr;
    if (verbose_gc) {
        fprintf(stderr, "%.2f heap used\n", pct_full());
    }
    if (pct_full() > 0.7f) {
        void* dummy = 0;
        set_stack_low(&dummy);
        collect();
        set_stack_low(&dummy);
    }
}

typedef struct Trail {
    LispVal** val_ptr;
    LispVal** val_ptr2;
    struct Trail* next;
} Trail;
// Need to fit our trail node in a LispVal space...
_Static_assert(sizeof(Trail) <= sizeof(LispVal),
        "Trail must fit in space of a freed LispVal");

static struct {
    int size;
    int capacity;
    struct { int from_off; int to_off; } *data;
} copy_mapping;

static void cm_reset()
{
    if (copy_mapping.capacity == 0) {
        copy_mapping.data = malloc(1024 * 2 * sizeof(int));
        copy_mapping.capacity = 1024;
        if (!copy_mapping.data) { perror("out of memory"); abort(); }
    }
    copy_mapping.size = 0;
}

static void cm_add_mapping(int from_off, int to_off)
{
    if (copy_mapping.size >= copy_mapping.capacity) {
        copy_mapping.data =
            reallocf(copy_mapping.data,
                    2 * copy_mapping.capacity * 2 * sizeof(int));
        copy_mapping.capacity *= 2;
        if (!copy_mapping.data) { perror("out of memory"); abort(); }
    }
    copy_mapping.data[copy_mapping.size].from_off = from_off;
    copy_mapping.data[copy_mapping.size].to_off = to_off;
    copy_mapping.size += 1;
}

static void copy_and_trace_value(
    LispVal**   current,
    Trail*      trail_start,
    LispVal***  rest_of_roots,
    int         nrest
);

void collect()
{
    // Need:
    // 1. Stack Roots
    // 2. Environment root
    // 3. Reader_stack
    if (verbose_gc)
        fprintf(stderr, "performing collection\n");

    void* old_free_ptr = free_ptr;
    heap_idx ^= 1; // Flip between 0 and 1
    free_ptr = heaps[heap_idx];

    int num_roots = 1; // 1 for &env - global environment

    // copy anything that is being read by the reader
    // The forms being read by the reader should just be directed-acyclic-trees
    // so hopefully nothing too complicated here
    tagged_stype* rs = reader_stack;
    tagged_stype* rsend = rs_ptr;


    for (tagged_stype* p = rs; p < rsend; p++) {
        if (p->tag == LISPVAL) {
            num_roots++;
        }
    }

    // And now scan our program stack for temporaries in the evaluator
    int num_heap_items = 0;
    for (void** it = stack_ref_low; it < stack_ref_high; ++it) {
        if (*it >= heaps[heap_idx ^ 1] && *it < old_free_ptr) {
            num_heap_items++;

            // assume this is a LispVal
            LispVal** lvref = (LispVal**)it;
            int tag = (*lvref)->tag;
            if (tag >= 0 && tag <= LERROR) {
                if (verbose_gc)
                    fprintf(stderr, "gc: found ref to %s\n", lv_tagname(*lvref));
                num_roots++;
            }
        }
    }
    if (verbose_gc) {
        fprintf(stderr, "gc: found %d old heap ptrs on stack\n", num_heap_items);
    }

    // Allocate array for roots
    LispVal*** roots = malloc(num_roots * sizeof(*roots));
    if (!roots) { perror("out of memory"); abort(); }
    LispVal*** roots_ptr = roots;

    // collate ptrs from reader stack
    for (tagged_stype* p = rs; p < rsend; p++) {
        if (p->tag == LISPVAL) {
            *roots_ptr++ = &p->sval.value;
        }
    }
    // And the values we found on the stack
    for (void** it = stack_ref_low; it < stack_ref_high; ++it) {
        if (*it >= heaps[heap_idx ^ 1] && *it < old_free_ptr) {
            LispVal** lvref = (LispVal**)it;
            int tag = (*lvref)->tag;
            if (tag >= 0 && tag <= LERROR) {
                *roots_ptr++ = lvref;
            }
        }
    }
    // include global environment
    *roots_ptr++ = &env;

    cm_reset();

    for (int i = 0; i < num_roots; i++) {
        // We may have nulled out our ref to aliases that have already been
        // copied, so skip those
        if (roots[i] != NULL) {
            /*
            * it's possible for any of the other roots to alias part of the tree
            * pointed to by a different root, so pass a list of other roots to
            * check for aliasing as we go
            */
            LispVal*** rest_of_roots = &roots[i + 1];
            int rest_of_roots_count = num_roots - i - 1;
            copy_and_trace_value(roots[i], NULL,
                    rest_of_roots, rest_of_roots_count);
        }
    }

    if (verbose_gc) {
        fprintf(stderr, "gc: collection finished\n");
        fprintf(stderr, "%.2f heap used\n", pct_full());
    }
    gc_stats.num_collections++;
    gc_stats.total_bytes_retained += (free_ptr - heaps[heap_idx]);
}

void copy_and_trace_value(
        LispVal** current, Trail* trail_start,
        LispVal*** rest_of_roots, int nrest)
{
    for (;;) {
        if (((void*)*current) >= heaps[heap_idx]
                && ((void*)*current) < free_ptr) {
            fprintf(stderr, "warning: this one has already been moved");
            // Not sure if to return here or not..
        }

        const int tag = (*current)->tag;
        if (!(tag >= 0 && tag <= LERROR)) {
            if (verbose_gc)
                fprintf(stderr, "gc: bad tag: %d (0x%x)\n", tag, tag);
            // Look up in copy_mapping
            int offset = ((void*)*current) - heaps[heap_idx ^ 1];
            int found = 0;
            for (int i = 0; i < copy_mapping.size; i++) {
                if (offset == copy_mapping.data[i].from_off) {
                    *current =
                        heaps[heap_idx] + copy_mapping.data[i].to_off;
                    found = 1;
                    break;
                }
            }
            if (verbose_gc) {
                if (found) {
                    fprintf(stderr, "gc: had already been moved but this "
                            "was an alias\n");
                } else {
                    fprintf(stderr, "gc: not found mapping into other heap!\n");
                }
            }
        } else {
            // 1. Copy value
            // 2. Copy the things it points to
            memcpy(free_ptr, *current, sizeof **current);

            // remember where we mapped this address
            cm_add_mapping(
                    ((void*)*current) - heaps[heap_idx ^ 1],
                    free_ptr - heaps[heap_idx]);

            void* spare_space = *current; /* Save the space we've just freed
                                            for use in this algorithm */

            *current = free_ptr;
            free_ptr += sizeof **current;
            ALIGNPTR(free_ptr);

            if (tag == LCONS) {
                if (verbose_gc)
                    fprintf(stderr, "it's a cons, follow head, save tail\n");
                // -- save a pointer to the tail somewhere
                // we can use the space we just made by copying current
                // to store a linked list containing the tail pointers
                // that we need to come back to
                Trail* trail_head = spare_space;
                trail_head->val_ptr = &(*current)->tail;
                trail_head->val_ptr2 = NULL;
                trail_head->next = trail_start;
                trail_start = trail_head;

                // stop-copy the head
                current = &(*current)->head;
                // Allow us to loop! (would be a recurse in a functional
                // language)
                continue;
            } else if (tag == LLAM || tag == LMAC) {
                if (verbose_gc)
                    fprintf(stderr, "it's a lambda, follow params, save body "
                            "and closure\n");
                Trail* trail_head = spare_space;
                trail_head->val_ptr = &(*current)->body;
                trail_head->val_ptr2 = &(*current)->closure;
                trail_head->next = trail_start;
                trail_start = trail_head;

                // next copy params
                current = &(*current)->params;
                // recurse!
                continue;
            }
        }

        if (trail_start) {
            if (verbose_gc)
                fprintf(stderr, "it's a %s, clean up our saved tails\n",
                    lv_tagname(*current));
            // Clean up the trail mess we've made
            // Assume we can do this in any order - just take the head
            current = trail_start->val_ptr;
            if (trail_start->val_ptr2) {
                trail_start->val_ptr = trail_start->val_ptr2;
                trail_start->val_ptr2 = NULL;
            } else {
                trail_start = trail_start->next;
            }
            // recurse! (or loop as it's known in c :P )
        } else {
            if (verbose_gc)
                fprintf(stderr, "it's a %s, nothing left on this trail\n",
                    lv_tagname(*current));
            // No mess and we are not a CONS! DONE! (for this item)
            break;
        }
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

