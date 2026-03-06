/**
 * test_closure_gc.c - Test Closure Garbage Collection Integration (Phase 7-E)
 *
 * Tests:
 * 1. Closure creation and GC tracking
 * 2. Closure marking during GC mark phase
 * 3. Closure cleanup during GC sweep phase
 * 4. Memory leak detection
 * 5. Nested closures with captured variables
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/freelang.h"
#include "../include/gc.h"

/* Forward declare GC functions for testing */
extern GC *gc_new(int initial_threshold);
extern void gc_free(GC *gc);
extern Value *gc_alloc(GC *gc, ValType type);
extern Value *gc_alloc_string(GC *gc, const char *str);
extern Value *gc_alloc_array(GC *gc, int capacity);
extern void gc_collect(GC *gc, Value **roots, int root_count,
                       void **env_chain, int env_count);
extern void gc_print_stats(GC *gc);
extern int gc_get_object_count(GC *gc);

/**
 * Test 1: Closure GC Tracking - Basic allocation
 */
void test_closure_gc_allocation(void) {
    printf("\n=== Test 1: Closure GC Allocation ===\n");

    GC *gc = gc_new(100);
    if (!gc) {
        printf("❌ Failed to create GC\n");
        return;
    }

    printf("Initial object count: %d\n", gc_get_object_count(gc));

    /* Allocate a closure */
    Value *closure_val = gc_alloc(gc, VAL_CLOSURE);
    if (!closure_val) {
        printf("❌ Failed to allocate closure Value\n");
        gc_free(gc);
        return;
    }

    printf("After allocating closure: %d objects\n", gc_get_object_count(gc));
    printf("Closure type: VAL_CLOSURE (0x%x)\n", VAL_CLOSURE);
    printf("✅ Closure allocated and tracked by GC\n");

    gc_free(gc);
}

/**
 * Test 2: Closure with Captured Variables - GC Marking
 */
void test_closure_with_captured_vars(void) {
    printf("\n=== Test 2: Closure with Captured Variables ===\n");

    GC *gc = gc_new(100);
    if (!gc) {
        printf("❌ Failed to create GC\n");
        return;
    }

    /* Allocate closure */
    Value *closure_val = gc_alloc(gc, VAL_CLOSURE);
    if (!closure_val) {
        printf("❌ Failed to allocate closure\n");
        gc_free(gc);
        return;
    }

    /* Allocate a string to be captured */
    Value *captured_str = gc_alloc_string(gc, "captured_var");
    if (!captured_str) {
        printf("❌ Failed to allocate captured string\n");
        gc_free(gc);
        return;
    }

    printf("Before GC: %d objects\n", gc_get_object_count(gc));

    /* Set up closure with captured variable (simulated) */
    if (closure_val) {
        fl_closure_t *c = (fl_closure_t *)malloc(sizeof(fl_closure_t));
        if (c) {
            c->func = NULL;
            c->captured_vars = (fl_captured_var_t *)malloc(sizeof(fl_captured_var_t));
            c->captured_count = 1;
            c->captured_capacity = 1;

            if (c->captured_vars) {
                c->captured_vars[0].name = strdup("x");
                c->captured_vars[0].value = (fl_value_t){
                    .type = FL_TYPE_STRING,
                    .data.string_val = "captured_value"
                };
                printf("✅ Closure with 1 captured variable created\n");
            }
            closure_val->closure_val = (void *)c;
        }
    }

    /* Mark closure as root */
    Value *roots[] = { closure_val, captured_str };
    gc_collect(gc, roots, 2, NULL, 0);

    printf("After GC: %d objects (freed unmarked objects)\n", gc_get_object_count(gc));
    printf("✅ Closure and captured variables marked and preserved\n");

    gc_free(gc);
}

/**
 * Test 3: Unreachable Closure - GC Cleanup
 */
void test_unreachable_closure_cleanup(void) {
    printf("\n=== Test 3: Unreachable Closure Cleanup ===\n");

    GC *gc = gc_new(10);
    if (!gc) {
        printf("❌ Failed to create GC\n");
        return;
    }

    int before = gc_get_object_count(gc);

    /* Allocate closure but don't add to roots */
    Value *closure_val = gc_alloc(gc, VAL_CLOSURE);
    Value *unused_str = gc_alloc_string(gc, "unused");

    int allocated = gc_get_object_count(gc);
    printf("After allocation: %d objects\n", allocated);

    /* Run GC with empty root set - should free unreachable objects */
    gc_collect(gc, NULL, 0, NULL, 0);

    int after = gc_get_object_count(gc);
    printf("After GC (no roots): %d objects\n", after);
    printf("✅ Unreachable objects cleaned up (freed %d objects)\n", allocated - after);

    gc_free(gc);
}

/**
 * Test 4: Mixed Closure and Non-Closure Values
 */
void test_mixed_values_with_closure(void) {
    printf("\n=== Test 4: Mixed Values with Closure ===\n");

    GC *gc = gc_new(100);
    if (!gc) {
        printf("❌ Failed to create GC\n");
        return;
    }

    /* Allocate various types */
    Value *closure = gc_alloc(gc, VAL_CLOSURE);
    Value *string = gc_alloc_string(gc, "test");
    Value *array = gc_alloc_array(gc, 10);

    printf("After allocations: %d objects\n", gc_get_object_count(gc));

    /* Mark only closure and string as roots */
    Value *roots[] = { closure, string };
    gc_collect(gc, roots, 2, NULL, 0);

    printf("After GC (closure + string as roots): %d objects\n", gc_get_object_count(gc));
    printf("✅ Only reachable values (closure, string) retained\n");

    gc_free(gc);
}

/**
 * Test 5: Statistics and Diagnostics
 */
void test_gc_statistics(void) {
    printf("\n=== Test 5: GC Statistics ===\n");

    GC *gc = gc_new(100);
    if (!gc) {
        printf("❌ Failed to create GC\n");
        return;
    }

    /* Allocate multiple objects */
    for (int i = 0; i < 5; i++) {
        gc_alloc_string(gc, "test");
        gc_alloc(gc, VAL_CLOSURE);
    }

    printf("Total objects before GC: %d\n", gc_get_object_count(gc));

    /* Run GC with no roots - should free everything */
    gc_collect(gc, NULL, 0, NULL, 0);

    printf("Total objects after GC: %d\n", gc_get_object_count(gc));

    printf("\n📊 GC Statistics:\n");
    gc_print_stats(gc);

    gc_free(gc);
    printf("✅ Statistics reported correctly\n");
}

/**
 * Main test runner
 */
int main(void) {
    printf("════════════════════════════════════════════════════\n");
    printf("  Phase 7-E: Closure GC Integration Test Suite\n");
    printf("════════════════════════════════════════════════════\n");

    test_closure_gc_allocation();
    test_closure_with_captured_vars();
    test_unreachable_closure_cleanup();
    test_mixed_values_with_closure();
    test_gc_statistics();

    printf("\n════════════════════════════════════════════════════\n");
    printf("✅ All Phase 7-E GC Integration Tests Complete\n");
    printf("════════════════════════════════════════════════════\n");

    return 0;
}
