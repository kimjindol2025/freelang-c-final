/**
 * FreeLang Closure Implementation
 * Phase 7: Lexical environment capture and closure execution
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../include/closure.h"
#include "../include/freelang.h"

/* ============================================================================
   Closure Creation & Management
   ============================================================================ */

fl_closure_t* fl_closure_create(fl_function_t* func,
                                 fl_captured_var_t* captured_vars,
                                 size_t captured_count) {
    if (!func) {
        fprintf(stderr, "[ERROR] Cannot create closure with NULL function\n");
        return NULL;
    }

    fl_closure_t* closure = (fl_closure_t*)malloc(sizeof(fl_closure_t));
    if (!closure) {
        fprintf(stderr, "[ERROR] Failed to allocate closure\n");
        return NULL;
    }

    closure->func = func;
    closure->captured_count = 0;
    closure->captured_capacity = 16;  /* Initial capacity */

    /* Allocate captured vars array */
    closure->captured_vars = (fl_captured_var_t*)malloc(16 * sizeof(fl_captured_var_t));
    if (!closure->captured_vars) {
        fprintf(stderr, "[ERROR] Failed to allocate captured variables array\n");
        free(closure);
        return NULL;
    }

    /* Copy captured variables */
    for (size_t i = 0; i < captured_count; i++) {
        if (fl_closure_capture_var(closure, captured_vars[i].name, captured_vars[i].value) != 0) {
            fprintf(stderr, "[ERROR] Failed to capture variable: %s\n", captured_vars[i].name);
            fl_closure_destroy(closure);
            return NULL;
        }
    }

    return closure;
}

void fl_closure_destroy(fl_closure_t* closure) {
    if (!closure) return;

    /* Free captured variable names */
    for (size_t i = 0; i < closure->captured_count; i++) {
        if (closure->captured_vars[i].name) {
            free(closure->captured_vars[i].name);
        }
        /* Note: We don't free the values themselves - GC handles that */
    }

    /* Free captured vars array */
    if (closure->captured_vars) {
        free(closure->captured_vars);
    }

    /* Note: We don't free the function - it may be shared */
    free(closure);
}

/* ============================================================================
   Captured Variables Management
   ============================================================================ */

int fl_closure_capture_var(fl_closure_t* closure, const char* name, fl_value_t value) {
    if (!closure || !name) {
        return -1;
    }

    /* Check if variable already captured - update if so */
    for (size_t i = 0; i < closure->captured_count; i++) {
        if (closure->captured_vars[i].name && strcmp(closure->captured_vars[i].name, name) == 0) {
            closure->captured_vars[i].value = value;
            return 0;
        }
    }

    /* Expand capacity if needed */
    if (closure->captured_count >= closure->captured_capacity) {
        closure->captured_capacity *= 2;
        fl_captured_var_t* new_vars = (fl_captured_var_t*)realloc(
            closure->captured_vars,
            closure->captured_capacity * sizeof(fl_captured_var_t)
        );
        if (!new_vars) {
            fprintf(stderr, "[ERROR] Failed to expand captured variables array\n");
            return -1;
        }
        closure->captured_vars = new_vars;
    }

    /* Add new variable */
    closure->captured_vars[closure->captured_count].name = (char*)malloc(strlen(name) + 1);
    if (!closure->captured_vars[closure->captured_count].name) {
        fprintf(stderr, "[ERROR] Failed to allocate variable name\n");
        return -1;
    }

    strcpy(closure->captured_vars[closure->captured_count].name, name);
    closure->captured_vars[closure->captured_count].value = value;
    closure->captured_count++;

    return 0;
}

int fl_closure_get_var(fl_closure_t* closure, const char* name, fl_value_t* out_value) {
    if (!closure || !name || !out_value) {
        return -1;
    }

    for (size_t i = 0; i < closure->captured_count; i++) {
        if (closure->captured_vars[i].name && strcmp(closure->captured_vars[i].name, name) == 0) {
            *out_value = closure->captured_vars[i].value;
            return 1;
        }
    }

    return 0;  /* Not found */
}

int fl_closure_set_var(fl_closure_t* closure, const char* name, fl_value_t value) {
    if (!closure || !name) {
        return -1;
    }

    for (size_t i = 0; i < closure->captured_count; i++) {
        if (closure->captured_vars[i].name && strcmp(closure->captured_vars[i].name, name) == 0) {
            closure->captured_vars[i].value = value;
            return 0;
        }
    }

    return -1;  /* Not found */
}

/* ============================================================================
   Value Type Conversions
   ============================================================================ */

fl_value_t fl_value_from_closure(fl_closure_t* closure) {
    fl_value_t value;
    value.type = FL_TYPE_CLOSURE;
    value.data.closure_val = closure;
    return value;
}

fl_closure_t* fl_value_to_closure(fl_value_t value) {
    if (value.type != FL_TYPE_CLOSURE) {
        return NULL;
    }
    return value.data.closure_val;
}

int fl_value_is_closure(fl_value_t value) {
    return value.type == FL_TYPE_CLOSURE;
}

/* ============================================================================
   Higher-Order Function Support
   ============================================================================ */

fl_value_t fl_closure_call(fl_closure_t* closure, fl_value_t* args, size_t arg_count) {
    fl_value_t error;
    error.type = FL_TYPE_ERROR;

    if (!closure || !closure->func) {
        fprintf(stderr, "[ERROR] Cannot call NULL closure\n");
        error.data.error_val = NULL;
        return error;
    }

    /* Check arity */
    if (arg_count != closure->func->arity) {
        fprintf(stderr, "[ERROR] Closure expects %zu arguments, got %zu\n",
                closure->func->arity, arg_count);
        error.data.error_val = NULL;
        return error;
    }

    /* TODO: Execute bytecode with captured environment
     * This will be implemented in VM with support for closure frames
     */

    return error;
}

int fl_value_is_callable(fl_value_t value) {
    return value.type == FL_TYPE_FUNCTION || value.type == FL_TYPE_CLOSURE;
}

fl_value_t fl_value_call(fl_value_t callable, fl_value_t* args, size_t arg_count) {
    if (callable.type == FL_TYPE_CLOSURE) {
        return fl_closure_call(callable.data.closure_val, args, arg_count);
    } else if (callable.type == FL_TYPE_FUNCTION) {
        /* TODO: Call native function or bytecode function */
    }

    fl_value_t error;
    error.type = FL_TYPE_ERROR;
    error.data.error_val = NULL;
    return error;
}
