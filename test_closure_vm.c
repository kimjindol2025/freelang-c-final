/**
 * Test VM Closure Execution
 * Phase 7-C: Test the new closure opcodes
 */

#include <stdio.h>
#include <stdlib.h>
#include "../include/freelang.h"
#include "../include/vm.h"
#include "../include/closure.h"
#include "../include/compiler.h"

int main() {
    printf("=== Testing VM Closure Execution ===\n\n");

    /* Test 1: Create and execute a simple closure */
    printf("Test 1: Simple Closure Creation\n");
    printf("Expected: Closure created with captured value\n\n");

    /* Create a function */
    fl_function_t func = {
        .name = "test_fn",
        .arity = 1,
        .is_native = false,
        .native_func = NULL,
        .bytecode = NULL,
        .bytecode_size = 0,
        .consts = NULL,
        .const_count = 0
    };

    /* Create captured variables */
    fl_captured_var_t captured[1];
    captured[0].name = "x";
    fl_value_t val;
    val.type = FL_TYPE_INT;
    val.data.int_val = 42;
    captured[0].value = val;

    /* Create closure */
    fl_closure_t* closure = fl_closure_create(&func, captured, 1);
    if (closure) {
        printf("✅ Closure created successfully\n");
        printf("   Function: %s\n", closure->func->name);
        printf("   Captured count: %zu\n", closure->captured_count);
        if (closure->captured_count > 0) {
            printf("   First captured var: %s = %lld\n",
                   closure->captured_vars[0].name,
                   closure->captured_vars[0].value.data.int_val);
        }

        /* Convert to value */
        fl_value_t closure_val = fl_value_from_closure(closure);
        printf("   Closure type: %d (FL_TYPE_CLOSURE = %d)\n",
               closure_val.type, FL_TYPE_CLOSURE);

        /* Extract back */
        fl_closure_t* extracted = fl_value_to_closure(closure_val);
        printf("   Extracted: %p (original: %p)\n", (void*)extracted, (void*)closure);
        printf("   Is closure? %d\n", fl_value_is_closure(closure_val));

        fl_closure_destroy(closure);
        printf("✅ Closure destroyed\n\n");
    } else {
        printf("❌ Failed to create closure\n\n");
    }

    /* Test 2: LOAD_UPVALUE and STORE_UPVALUE operations */
    printf("Test 2: Upvalue Operations\n");
    printf("Expected: Read and write captured variables\n\n");

    fl_vm_t* vm = fl_vm_create();
    if (vm) {
        /* Create a closure with one captured variable */
        fl_function_t func2 = {
            .name = "closure_fn",
            .arity = 0,
            .is_native = false,
            .native_func = NULL,
            .bytecode = NULL,
            .bytecode_size = 0,
            .consts = NULL,
            .const_count = 0
        };

        fl_captured_var_t captured2[1];
        captured2[0].name = "counter";
        fl_value_t counter_val;
        counter_val.type = FL_TYPE_INT;
        counter_val.data.int_val = 0;
        captured2[0].value = counter_val;

        fl_closure_t* closure2 = fl_closure_create(&func2, captured2, 1);
        if (closure2) {
            /* Set current closure in VM */
            vm->current_closure = closure2;

            printf("✅ Created closure with counter = 0\n");

            /* Test LOAD_UPVALUE (index 0) - should load the counter */
            printf("   Testing LOAD_UPVALUE...\n");
            fl_value_t loaded = vm->current_closure->captured_vars[0].value;
            printf("   Loaded value: %lld\n", loaded.data.int_val);

            /* Test STORE_UPVALUE (index 0) - should store new value */
            printf("   Testing STORE_UPVALUE...\n");
            fl_value_t new_val;
            new_val.type = FL_TYPE_INT;
            new_val.data.int_val = 5;
            vm->current_closure->captured_vars[0].value = new_val;
            printf("   Stored value: %lld\n", vm->current_closure->captured_vars[0].value.data.int_val);

            /* Verify it was stored */
            fl_value_t verified = vm->current_closure->captured_vars[0].value;
            if (verified.data.int_val == 5) {
                printf("✅ Upvalue operations working correctly\n\n");
            } else {
                printf("❌ Upvalue operation failed\n\n");
            }

            fl_closure_destroy(closure2);
        } else {
            printf("❌ Failed to create closure2\n\n");
        }

        fl_vm_destroy(vm);
    } else {
        printf("❌ Failed to create VM\n\n");
    }

    printf("=== Test Complete ===\n");
    return 0;
}
