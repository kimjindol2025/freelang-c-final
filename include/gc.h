/**
 * FreeLang C Runtime - Garbage Collector (gc.h)
 *
 * Mark-and-Sweep GC implementation for FreeLang runtime values
 * Integrates with myos-lib (Vector, HashMap, Memory Manager)
 *
 * Features:
 * - Value type system (NULL, BOOL, NUMBER, STRING, ARRAY, OBJECT, FUNCTION)
 * - Mark-and-sweep garbage collection
 * - String interning via HashMap
 * - Root set tracking (stack, environment chain)
 * - GC statistics and diagnostics
 *
 * 2026-03-06
 */

#ifndef FREELANG_GC_H
#define FREELANG_GC_H

#include <stddef.h>
#include <stdint.h>

/* Forward declarations */
typedef struct Value Value;
typedef struct VM VM;

/* ============================================================================
 * Value Type System
 * ============================================================================ */

/**
 * ValType - Value type enumeration
 *
 * VAL_NULL:      null/undefined
 * VAL_BOOL:      boolean (true/false)
 * VAL_NUMBER:    double floating-point
 * VAL_STRING:    interned string (ref to global string pool)
 * VAL_ARRAY:     dynamic array (Vector* from myos-lib)
 * VAL_OBJECT:    key-value store (HashMap* from myos-lib)
 * VAL_FUNCTION:  user-defined or built-in function
 * VAL_NATIVE_FN: C function pointer
 * VAL_CLOSURE:   closure with captured environment (Phase 7)
 */
typedef enum {
    VAL_NULL,
    VAL_BOOL,
    VAL_NUMBER,
    VAL_STRING,
    VAL_ARRAY,
    VAL_OBJECT,
    VAL_FUNCTION,
    VAL_NATIVE_FN,
    VAL_CLOSURE     /* NEW: Closure with captured variables */
} ValType;

/* ============================================================================
 * Value Structure
 * ============================================================================ */

/**
 * Value - Runtime value representation
 *
 * All heap objects (STRING, ARRAY, OBJECT, FUNCTION) are GC-managed.
 * Stack values (BOOL, NUMBER, NULL) don't require GC but are unified.
 *
 * is_marked:    Mark bit for GC (0=unmarked, 1=marked in mark phase)
 * type:         Type discriminator for union access
 * union:        Type-specific data payload
 */
typedef struct Value {
    ValType type;
    int is_marked;      /* GC mark bit (0 or 1) */

    union {
        /* Scalar types */
        int bool_val;           /* VAL_BOOL: 0=false, 1=true */
        double num_val;         /* VAL_NUMBER: IEEE-754 double */

        /* Heap types */
        char *str_val;          /* VAL_STRING: interned string (never NULL) */
        void *array_val;        /* VAL_ARRAY: Vector* from myos-lib */
        void *obj_val;          /* VAL_OBJECT: HashMap* from myos-lib */

        /* Function types */
        void *func_ptr;         /* VAL_FUNCTION: user function metadata */
        void (*native_fn_val)(VM *vm, Value **args, int argc);
                                /* VAL_NATIVE_FN: C function pointer */

        /* Closure type (Phase 7) */
        void *closure_val;      /* VAL_CLOSURE: fl_closure_t* with captured variables */
    };
} Value;

/* ============================================================================
 * GC Structure
 * ============================================================================ */

/**
 * GC - Garbage collector state
 *
 * Tracks all heap-allocated Value objects for mark-and-sweep collection.
 * Uses a simple array (all_objects) for fast iteration during mark/sweep.
 *
 * all_objects:     Array of all allocated Value* pointers
 * object_count:    Current number of allocated objects
 * object_capacity: Allocated array size
 * gc_threshold:    Trigger GC when object_count exceeds this
 * gc_count:        Number of GC collections performed (stats)
 * total_freed:     Total objects freed by GC (stats)
 */
typedef struct GC_struct {
    Value **all_objects;        /* Heap array of Value* */
    int object_count;           /* Current count */
    int object_capacity;        /* Allocated slots */
    int gc_threshold;           /* Trigger level (default: 1024) */
    int gc_count;               /* Collections performed */
    int total_freed;            /* Objects freed lifetime */
} GC;

/* ============================================================================
 * GC Initialization and Cleanup
 * ============================================================================ */

/**
 * gc_new - Create a new garbage collector
 *
 * @param initial_threshold: GC trigger threshold (object count)
 *                           Typical: 1024
 * @return: New GC* or NULL on failure
 *
 * Initializes GC state. Should be called once at VM startup.
 * Uses myos-lib mm_alloc for internal structures.
 */
GC *gc_new(int initial_threshold);

/**
 * gc_free - Destroy garbage collector and all tracked objects
 *
 * @param gc: GC instance to destroy (may be NULL)
 *
 * Frees all Value objects in all_objects array, then frees GC structure itself.
 * Should be called once at VM shutdown.
 */
void gc_free(GC *gc);

/* ============================================================================
 * Value Allocation Functions
 * ============================================================================ */

/**
 * gc_alloc - Allocate a new Value object of given type
 *
 * @param gc: GC instance
 * @param type: Value type (ValType enum)
 * @return: Initialized Value* with type set, or NULL on failure
 *
 * Allocates Value struct, adds to all_objects tracking array.
 * For scalar types (NULL, BOOL, NUMBER), minimal initialization.
 * For heap types (STRING, ARRAY, OBJECT), caller must initialize union.
 *
 * May trigger GC if object_count reaches gc_threshold.
 */
Value *gc_alloc(GC *gc, ValType type);

/**
 * gc_alloc_string - Allocate and intern a string
 *
 * @param gc: GC instance
 * @param str: C-string to intern (may be NULL)
 * @return: Value* with VAL_STRING type, or NULL on failure
 *
 * Creates VAL_STRING value and automatically interns the string.
 * String interning uses global string pool to deduplicate storage.
 * If str already exists in pool, returns reference to existing string.
 *
 * If str is NULL, returns VAL_STRING with empty string "".
 */
Value *gc_alloc_string(GC *gc, const char *str);

/**
 * gc_alloc_array - Allocate an array (Vector)
 *
 * @param gc: GC instance
 * @param capacity: Initial array capacity (elements)
 * @return: Value* with VAL_ARRAY type, or NULL on failure
 *
 * Creates VAL_ARRAY value backed by myos-lib Vector.
 * Vector stores Value* elements (allows nested arrays/objects).
 * Capacity is element count, not bytes.
 */
Value *gc_alloc_array(GC *gc, int capacity);

/**
 * gc_alloc_object - Allocate an object (HashMap)
 *
 * @param gc: GC instance
 * @return: Value* with VAL_OBJECT type, or NULL on failure
 *
 * Creates VAL_OBJECT value backed by myos-lib HashMap.
 * HashMap stores string keys (const char*) → Value* values.
 * Supports arbitrary nesting of objects and arrays.
 */
Value *gc_alloc_object(GC *gc);

/* ============================================================================
 * Mark-and-Sweep GC Algorithm
 * ============================================================================ */

/**
 * gc_mark_value - Mark a single value and all reachable values
 *
 * @param val: Value to mark (may be NULL, checked internally)
 *
 * Recursive marking:
 * - Scalar types (NULL, BOOL, NUMBER): no children, immediate return
 * - STRING: marks the string itself only (no children)
 * - ARRAY: marks array and recursively marks all element Values
 * - OBJECT: marks object and recursively marks all value Values (not keys)
 * - FUNCTION/NATIVE_FN: marks function object (implementation-specific)
 * - CLOSURE: marks closure and recursively marks all captured variables
 *
 * Sets is_marked = 1 during traversal.
 * Safe to call multiple times on same value.
 */
void gc_mark_value(Value *val);

/**
 * gc_mark_roots - Mark root set (stack + environment chain)
 *
 * @param gc: GC instance
 * @param roots: Array of Value* that are root references
 * @param root_count: Number of root Values
 * @param env_chain: Array of environment HashMaps (scope chain)
 * @param env_count: Number of environments
 *
 * Marks all values in:
 * 1. roots array (local variables, parameters)
 * 2. All values stored in env_chain HashMaps (closure scope)
 *
 * Called at start of gc_collect() to identify live objects.
 */
void gc_mark_roots(GC *gc, Value **roots, int root_count,
                   void **env_chain, int env_count);

/**
 * gc_sweep - Sweep phase: free unmarked objects
 *
 * @param gc: GC instance
 *
 * Iterates all_objects, checks is_marked bit:
 * - If marked=1: reset is_marked to 0 for next cycle
 * - If marked=0: free Value object, remove from all_objects array
 *
 * Updates gc->total_freed and gc->object_count.
 * Maintains all_objects array compactness (memmove on removal).
 */
void gc_sweep(GC *gc);

/**
 * gc_collect - Full GC cycle: mark + sweep
 *
 * @param gc: GC instance
 * @param roots: Array of root Value* pointers
 * @param root_count: Number of roots
 * @param env_chain: Array of environment HashMaps
 * @param env_count: Number of environments
 *
 * Main GC entry point. Performs complete Mark-and-Sweep:
 * 1. Mark phase: gc_mark_roots() then gc_mark_value() on all reachable
 * 2. Sweep phase: gc_sweep() frees unreachable objects
 *
 * Should be called:
 * - Automatically when object_count >= gc_threshold
 * - Explicitly by VM at strategic points (loop iterations, etc.)
 * - Before VM shutdown to release all objects
 */
void gc_collect(GC *gc, Value **roots, int root_count,
                void **env_chain, int env_count);

/* ============================================================================
 * GC Statistics and Diagnostics
 * ============================================================================ */

/**
 * gc_print_stats - Print GC statistics to stdout
 *
 * @param gc: GC instance
 *
 * Output format:
 *   GC Stats:
 *     Objects tracked: N/capacity
 *     Threshold: T
 *     Collections: C
 *     Total freed: F
 *     Estimated memory: X bytes
 *
 * Useful for performance analysis and debugging.
 */
void gc_print_stats(GC *gc);

/**
 * gc_get_object_count - Get current tracked object count
 *
 * @param gc: GC instance
 * @return: Number of live Value objects
 */
int gc_get_object_count(GC *gc);

/**
 * gc_get_collection_count - Get lifetime GC collection count
 *
 * @param gc: GC instance
 * @return: Number of gc_collect() calls executed
 */
int gc_get_collection_count(GC *gc);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * gc_is_heap_value - Check if value requires GC tracking
 *
 * @param type: Value type
 * @return: 1 if heap-allocated, 0 if scalar
 *
 * Heap types: STRING, ARRAY, OBJECT, FUNCTION
 * Scalar types: NULL, BOOL, NUMBER (no GC needed)
 */
int gc_is_heap_value(ValType type);

/**
 * gc_value_type_name - Get human-readable type name
 *
 * @param type: Value type
 * @return: String like "NULL", "BOOL", "ARRAY", etc.
 *
 * Useful for debugging and error messages.
 */
const char *gc_value_type_name(ValType type);

/**
 * gc_value_to_string - Convert value to string representation
 *
 * @param val: Value to convert
 * @param buf: Output buffer (caller-allocated)
 * @param buflen: Buffer size
 * @return: buf, or NULL on failure
 *
 * Converts value to human-readable string:
 * - NULL → "null"
 * - BOOL → "true" or "false"
 * - NUMBER → decimal string (may lose precision)
 * - STRING → quoted string contents
 * - ARRAY → "[...]" (not fully enumerated)
 * - OBJECT → "{...}" (not fully enumerated)
 * - FUNCTION → "[Function]"
 */
const char *gc_value_to_string(Value *val, char *buf, size_t buflen);

#endif /* FREELANG_GC_H */
