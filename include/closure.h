/**
 * FreeLang Closure & Higher-Order Function Support
 * Phase 7: Closures and lexical environment capture
 */

#ifndef FL_CLOSURE_H
#define FL_CLOSURE_H

#include "freelang.h"

/* ============================================================================
   Closure Creation & Management
   ============================================================================ */

/**
 * Create a new closure from a function with captured environment
 * @param func The underlying function
 * @param captured_vars Array of captured variable values
 * @param captured_count Number of captured variables
 * @return New closure, or NULL on error
 */
fl_closure_t* fl_closure_create(fl_function_t* func,
                                 fl_captured_var_t* captured_vars,
                                 size_t captured_count);

/**
 * Destroy a closure and free its resources
 */
void fl_closure_destroy(fl_closure_t* closure);

/* ============================================================================
   Captured Variables Management
   ============================================================================ */

/**
 * Capture a variable from the environment
 * @param closure Target closure
 * @param name Variable name
 * @param value Variable value
 * @return 0 on success, -1 on error
 */
int fl_closure_capture_var(fl_closure_t* closure, const char* name, fl_value_t value);

/**
 * Get captured variable by name
 * @param closure Target closure
 * @param name Variable name
 * @param out_value Output value (if found)
 * @return 1 if found, 0 if not found, -1 on error
 */
int fl_closure_get_var(fl_closure_t* closure, const char* name, fl_value_t* out_value);

/**
 * Set captured variable value
 * @param closure Target closure
 * @param name Variable name
 * @param value New value
 * @return 0 on success, -1 if not found
 */
int fl_closure_set_var(fl_closure_t* closure, const char* name, fl_value_t value);

/* ============================================================================
   Value Type Conversions
   ============================================================================ */

/**
 * Create fl_value from closure
 */
fl_value_t fl_value_from_closure(fl_closure_t* closure);

/**
 * Extract closure from fl_value
 */
fl_closure_t* fl_value_to_closure(fl_value_t value);

/**
 * Check if value is a closure
 */
int fl_value_is_closure(fl_value_t value);

/* ============================================================================
   Higher-Order Function Support
   ============================================================================ */

/**
 * Call a closure with arguments
 * @param closure The closure to call
 * @param args Array of arguments
 * @param arg_count Number of arguments
 * @return Result value (may contain error)
 */
fl_value_t fl_closure_call(fl_closure_t* closure, fl_value_t* args, size_t arg_count);

/**
 * Check if a value is callable (function or closure)
 */
int fl_value_is_callable(fl_value_t value);

/**
 * Call any callable value (function or closure)
 */
fl_value_t fl_value_call(fl_value_t callable, fl_value_t* args, size_t arg_count);

#endif /* FL_CLOSURE_H */
