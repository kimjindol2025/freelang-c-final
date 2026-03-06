/**
 * FreeLang C Runtime - Garbage Collector (gc.c)
 *
 * Mark-and-Sweep implementation for FreeLang runtime
 * Integrates with myos-lib: Vector, HashMap, Memory Manager
 *
 * Architecture:
 * - All heap Values tracked in GC.all_objects array
 * - Mark phase: traverse reachable values from roots
 * - Sweep phase: free unmarked values
 * - String interning: global HashMap for deduplication
 *
 * 2026-03-06
 */

#include "../include/freelang.h"
#include "gc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations for myos-lib integration */
/* These would come from actual myos-lib headers in a real build */
typedef struct {
    void **elements;
    int size;
    int capacity;
    int elem_size;
} Vector;

typedef struct {
    void *entries;
    size_t capacity;
    size_t count;
    size_t key_size;
    size_t value_size;
    uint32_t (*hash_func)(const void *key, size_t key_size);
    int (*cmp_func)(const void *key1, const void *key2);
} HashMap;

/* Global string interning pool (reserved for future optimization) */
/* static HashMap *g_string_pool = NULL; */
static int g_string_pool_initialized = 0;

/* ============================================================================
 * String Interning Implementation
 * ============================================================================ */

/**
 * Simple hash function for C strings (reserved for future use)
 */
/* static uint32_t string_hash(const void *key, size_t key_size) {
    (void)key_size;  // Suppress unused parameter warning
    const char *str = (const char *)key;
    uint32_t hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) ^ c;  // hash = hash*33 ^ c
    }

    return hash;
} */

/**
 * Comparison function for C strings (reserved for future use)
 */
/* static int string_cmp(const void *key1, const void *key2) {
    return strcmp((const char *)key1, (const char *)key2);
} */

/**
 * Initialize global string pool (lazy initialization)
 */
static int init_string_pool(void) {
    if (g_string_pool_initialized) {
        return 0;
    }

    /* Create HashMap with string keys and string values */
    /* In real implementation, use actual myos-lib hash_new */
    /* For now, simulate with a simple implementation */

    g_string_pool_initialized = 1;
    return 0;
}

/**
 * Intern a string - return pointer to unique string in pool
 *
 * Returns pointer to interned string (allocated once, reused thereafter).
 * If string already exists, returns existing pointer.
 * Otherwise allocates new string and stores in pool.
 */
static char *intern_string(const char *str) {
    if (!str) {
        str = "";  /* NULL becomes empty string */
    }

    /* In real implementation:
     * 1. Check if str exists in g_string_pool
     * 2. If yes, return pointer to existing
     * 3. If no, allocate new string, store in pool, return
     *
     * For this implementation, we'll use a simpler approach:
     * allocate fresh copy each time (non-optimal but functional)
     */

    int len = strlen(str);
    char *interned = (char *)malloc(len + 1);
    if (!interned) {
        return NULL;
    }
    strcpy(interned, str);
    return interned;
}

/* ============================================================================
 * Vector Stub Functions (from myos-lib)
 * ============================================================================ */

/**
 * Create a new Vector
 * Real implementation uses myos-lib vector_new()
 */
static Vector *vector_new(int capacity, int elem_size) {
    Vector *v = (Vector *)malloc(sizeof(Vector));
    if (!v) return NULL;

    v->elem_size = elem_size;
    v->capacity = capacity > 0 ? capacity : 16;
    v->size = 0;
    v->elements = (void **)malloc(v->capacity * sizeof(void *));

    if (!v->elements) {
        free(v);
        return NULL;
    }

    return v;
}

/**
 * Free a Vector and all its elements
 */
static void vector_free(Vector *v) {
    if (!v) return;
    if (v->elements) {
        free(v->elements);
    }
    free(v);
}

/**
 * Get element from Vector by index
 */
static void *vector_get(Vector *v, int idx) {
    if (!v || idx < 0 || idx >= v->size) {
        return NULL;
    }
    return v->elements[idx];
}

/**
 * Get size of Vector
 */
static int vector_size(Vector *v) {
    return v ? v->size : 0;
}

/**
 * Push element to Vector (reserved for future use)
 */
/* static int vector_push(Vector *v, void *elem) {
    if (!v) return -1;

    if (v->size >= v->capacity) {
        int new_capacity = v->capacity * 2;
        void **new_elements = (void **)realloc(v->elements,
                                                new_capacity * sizeof(void *));
        if (!new_elements) {
            return -1;
        }
        v->elements = new_elements;
        v->capacity = new_capacity;
    }

    v->elements[v->size++] = elem;
    return 0;
} */

/* ============================================================================
 * HashMap Stub Functions (from myos-lib)
 * ============================================================================ */

/**
 * Create a new HashMap
 * Real implementation uses myos-lib hash_new()
 */
static HashMap *hashmap_new(size_t capacity) {
    HashMap *h = (HashMap *)malloc(sizeof(HashMap));
    if (!h) return NULL;

    h->capacity = capacity > 0 ? capacity : 16;
    h->count = 0;
    h->entries = (void *)malloc(h->capacity * (64 + 64));  /* key + value */

    if (!h->entries) {
        free(h);
        return NULL;
    }

    return h;
}

/**
 * Free HashMap
 */
static void hashmap_free(HashMap *h) {
    if (!h) return;
    if (h->entries) {
        free(h->entries);
    }
    free(h);
}

/**
 * Get value from HashMap by string key (reserved for future use)
 * Returns pointer to value, or NULL if not found
 */
/* static void *hashmap_get(HashMap *h, const char *key) {
    if (!h || !key) return NULL;
    // Stub: in real implementation, search entries array
    return NULL;
} */

/**
 * Get all values from HashMap (for GC marking)
 * Returns array of value pointers
 */
static void **hashmap_values(HashMap *h, int *out_count) {
    if (!h) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    /* Stub: in real implementation, extract all values */
    if (out_count) *out_count = 0;
    return NULL;
}

/* ============================================================================
 * GC Core Functions
 * ============================================================================ */

/**
 * gc_new - Create new garbage collector instance
 */
GC *gc_new(int initial_threshold) {
    if (initial_threshold <= 0) {
        initial_threshold = 1024;
    }

    GC *gc = (GC *)malloc(sizeof(GC));
    if (!gc) {
        return NULL;
    }

    /* Initialize tracking array */
    gc->object_capacity = 256;  /* Start with 256 slots */
    gc->all_objects = (Value **)malloc(gc->object_capacity * sizeof(Value *));

    if (!gc->all_objects) {
        free(gc);
        return NULL;
    }

    gc->object_count = 0;
    gc->gc_threshold = initial_threshold;
    gc->gc_count = 0;
    gc->total_freed = 0;

    /* Initialize string pool */
    init_string_pool();

    return gc;
}

/**
 * gc_free - Destroy GC and all tracked objects
 */
void gc_free(GC *gc) {
    if (!gc) {
        return;
    }

    /* Free all tracked Value objects */
    for (int i = 0; i < gc->object_count; i++) {
        Value *val = gc->all_objects[i];
        if (!val) continue;

        /* Free heap resources for specific types */
        switch (val->type) {
            case VAL_STRING:
                if (val->str_val) {
                    free(val->str_val);
                }
                break;
            case VAL_ARRAY:
                if (val->array_val) {
                    vector_free((Vector *)val->array_val);
                }
                break;
            case VAL_OBJECT:
                if (val->obj_val) {
                    hashmap_free((HashMap *)val->obj_val);
                }
                break;
            default:
                break;
        }

        free(val);
    }

    /* Free tracking array */
    if (gc->all_objects) {
        free(gc->all_objects);
    }

    free(gc);
}

/**
 * gc_alloc - Allocate a new Value of given type
 */
Value *gc_alloc(GC *gc, ValType type) {
    if (!gc) {
        return NULL;
    }

    /* Allocate Value structure */
    Value *val = (Value *)malloc(sizeof(Value));
    if (!val) {
        return NULL;
    }

    /* Initialize */
    val->type = type;
    val->is_marked = 0;
    memset(&val->bool_val, 0, sizeof(val->bool_val));  /* Clear union */

    /* Add to tracking array */
    if (gc->object_count >= gc->object_capacity) {
        /* Resize tracking array */
        int new_capacity = gc->object_capacity * 2;
        Value **new_objects = (Value **)realloc(gc->all_objects,
                                                 new_capacity * sizeof(Value *));
        if (!new_objects) {
            free(val);
            return NULL;
        }

        gc->all_objects = new_objects;
        gc->object_capacity = new_capacity;
    }

    gc->all_objects[gc->object_count++] = val;

    /* Check if GC should be triggered */
    if (gc->object_count >= gc->gc_threshold) {
        /* In real implementation, signal VM to trigger GC */
        /* For now, just note that threshold reached */
    }

    return val;
}

/**
 * gc_alloc_string - Allocate interned string Value
 */
Value *gc_alloc_string(GC *gc, const char *str) {
    if (!gc) {
        return NULL;
    }

    Value *val = gc_alloc(gc, VAL_STRING);
    if (!val) {
        return NULL;
    }

    /* Intern the string */
    val->str_val = intern_string(str);
    if (!val->str_val && str != NULL) {
        /* Allocation failed, remove from GC tracking */
        gc->object_count--;
        free(val);
        return NULL;
    }

    return val;
}

/**
 * gc_alloc_array - Allocate array Value
 */
Value *gc_alloc_array(GC *gc, int capacity) {
    if (!gc) {
        return NULL;
    }

    Value *val = gc_alloc(gc, VAL_ARRAY);
    if (!val) {
        return NULL;
    }

    /* Create backing Vector (stores Value* elements) */
    Vector *arr = vector_new(capacity, sizeof(Value *));
    if (!arr) {
        gc->object_count--;
        free(val);
        return NULL;
    }

    val->array_val = (void *)arr;
    return val;
}

/**
 * gc_alloc_object - Allocate object Value
 */
Value *gc_alloc_object(GC *gc) {
    if (!gc) {
        return NULL;
    }

    Value *val = gc_alloc(gc, VAL_OBJECT);
    if (!val) {
        return NULL;
    }

    /* Create backing HashMap (string keys -> Value* values) */
    HashMap *obj = hashmap_new(16);
    if (!obj) {
        gc->object_count--;
        free(val);
        return NULL;
    }

    val->obj_val = (void *)obj;
    return val;
}

/* ============================================================================
 * Mark-and-Sweep Implementation
 * ============================================================================ */

/**
 * gc_mark_value - Mark single value and reachable children
 *
 * Recursively marks value and all reachable children:
 * - Scalar types: return immediately (no children)
 * - STRING: mark the string
 * - ARRAY: mark all element Values
 * - OBJECT: mark all value Values (keys are strings, not marked)
 * - FUNCTION: mark function object
 */
void gc_mark_value(Value *val) {
    if (!val || val->is_marked) {
        return;  /* Already marked or NULL, avoid infinite recursion */
    }

    /* Mark this value */
    val->is_marked = 1;

    /* Recursively mark children based on type */
    switch (val->type) {
        case VAL_NULL:
        case VAL_BOOL:
        case VAL_NUMBER:
        case VAL_NATIVE_FN:
            /* Scalar types have no children */
            break;

        case VAL_STRING:
            /* String interning: strings are shared, but is_marked on the Value */
            /* If we're using a separate string pool, mark that too */
            /* For now, just the Value itself is marked */
            break;

        case VAL_ARRAY: {
            /* Recursively mark all elements */
            Vector *arr = (Vector *)val->array_val;
            if (arr) {
                for (int i = 0; i < vector_size(arr); i++) {
                    Value *elem = (Value *)vector_get(arr, i);
                    gc_mark_value(elem);  /* Recursive marking */
                }
            }
            break;
        }

        case VAL_OBJECT: {
            /* Recursively mark all values (not keys, as keys are strings) */
            HashMap *obj = (HashMap *)val->obj_val;
            if (obj) {
                int count = 0;
                void **values = hashmap_values(obj, &count);
                for (int i = 0; i < count; i++) {
                    Value *v = (Value *)values[i];
                    gc_mark_value(v);  /* Recursive marking */
                }
                if (values) {
                    free(values);
                }
            }
            break;
        }

        case VAL_FUNCTION: {
            /* Function object: mark captured variables if closure */
            if (val->func_ptr) {
                /* Check if this is a closure by examining the function structure */
                /* If the function has captured variables, mark them recursively */
                /* Note: Closure integration - mark upvalues if present */

                /* For now, the function itself is marked, and any captured */
                /* variables in the closure would be part of the closure structure */
                /* This will be enhanced in Phase 7-E */
            }
            break;
        }

        case VAL_CLOSURE: {
            /* Closure object: mark captured variables recursively */
            if (val->closure_val) {
                fl_closure_t *closure = (fl_closure_t *)val->closure_val;

                /* Mark the underlying function */
                if (closure->func) {
                    /* The function itself doesn't have direct fl_value_t representation */
                    /* but we should mark any values it references */
                }

                /* Recursively mark all captured variables */
                if (closure->captured_vars && closure->captured_count > 0) {
                    for (size_t i = 0; i < closure->captured_count; i++) {
                        fl_captured_var_t *captured = &closure->captured_vars[i];

                        /* Each captured variable's value should be marked */
                        /* depending on its type representation */
                        /* For fl_value_t values, mark them directly */
                        if (captured->value.type == FL_TYPE_ARRAY ||
                            captured->value.type == FL_TYPE_OBJECT ||
                            captured->value.type == FL_TYPE_FUNCTION ||
                            captured->value.type == FL_TYPE_CLOSURE ||
                            captured->value.type == FL_TYPE_STRING) {
                            /* These types contain heap-allocated data that needs marking */
                            /* Convert fl_value_t to internal Value representation and mark */
                            /* This requires compatibility layer between fl_value_t and Value */
                        }
                    }
                }
            }
            break;
        }

        default:
            break;
    }
}

/**
 * gc_mark_roots - Mark root set (stack + environments)
 *
 * Marks:
 * 1. All values in roots array (local variables, parameters)
 * 2. All values in env_chain HashMaps (scope chain)
 */
void gc_mark_roots(GC *gc, Value **roots, int root_count,
                   void **env_chain, int env_count) {
    if (!gc) {
        return;
    }

    /* Mark all root values */
    for (int i = 0; i < root_count; i++) {
        if (roots && roots[i]) {
            gc_mark_value(roots[i]);
        }
    }

    /* Mark all values in environment chain */
    for (int i = 0; i < env_count; i++) {
        if (!env_chain || !env_chain[i]) {
            continue;
        }

        HashMap *env = (HashMap *)env_chain[i];
        int count = 0;
        void **values = hashmap_values(env, &count);

        for (int j = 0; j < count; j++) {
            if (values && values[j]) {
                gc_mark_value((Value *)values[j]);
            }
        }

        if (values) {
            free(values);
        }
    }
}

/**
 * gc_sweep - Free unmarked objects
 *
 * Iterates all_objects, checks is_marked:
 * - If marked: reset is_marked for next cycle, keep object
 * - If unmarked: free object, remove from all_objects
 *
 * Maintains all_objects compactness via array shifting.
 */
void gc_sweep(GC *gc) {
    if (!gc) {
        return;
    }

    int write_idx = 0;

    for (int read_idx = 0; read_idx < gc->object_count; read_idx++) {
        Value *val = gc->all_objects[read_idx];

        if (val->is_marked) {
            /* Keep this object, reset mark for next cycle */
            val->is_marked = 0;
            gc->all_objects[write_idx++] = val;
        } else {
            /* Free unmarked object */
            switch (val->type) {
                case VAL_STRING:
                    if (val->str_val) {
                        free(val->str_val);
                    }
                    break;

                case VAL_ARRAY:
                    if (val->array_val) {
                        vector_free((Vector *)val->array_val);
                    }
                    break;

                case VAL_OBJECT:
                    if (val->obj_val) {
                        hashmap_free((HashMap *)val->obj_val);
                    }
                    break;

                case VAL_CLOSURE: {
                    /* Free closure object and its captured variables */
                    if (val->closure_val) {
                        fl_closure_t *closure = (fl_closure_t *)val->closure_val;

                        /* Free captured variables array */
                        if (closure->captured_vars) {
                            /* Each captured variable's data should be freed */
                            /* depending on its type (strings, arrays, objects, etc.) */
                            for (size_t i = 0; i < closure->captured_count; i++) {
                                fl_captured_var_t *captured = &closure->captured_vars[i];

                                /* Free the variable name string if allocated */
                                if (captured->name) {
                                    free((char *)captured->name);
                                }

                                /* Free heap-allocated values */
                                switch (captured->value.type) {
                                    case FL_TYPE_STRING:
                                        if (captured->value.data.string_val) {
                                            free((char *)captured->value.data.string_val);
                                        }
                                        break;

                                    case FL_TYPE_ARRAY:
                                        if (captured->value.data.array_val) {
                                            /* Arrays may contain nested structures */
                                            /* Proper deallocation handled by runtime */
                                            free(captured->value.data.array_val);
                                        }
                                        break;

                                    case FL_TYPE_OBJECT:
                                        if (captured->value.data.object_val) {
                                            /* Objects may contain nested structures */
                                            free(captured->value.data.object_val);
                                        }
                                        break;

                                    default:
                                        /* Scalar types have no additional cleanup */
                                        break;
                                }
                            }

                            free(closure->captured_vars);
                            closure->captured_vars = NULL;
                            closure->captured_count = 0;
                            closure->captured_capacity = 0;
                        }

                        /* The underlying function pointer is not freed here */
                        /* as it may be shared between closures or referenced elsewhere */

                        /* Free the closure structure itself */
                        free(closure);
                    }
                    break;
                }

                case VAL_FUNCTION:
                    /* Function cleanup if needed */
                    /* Functions are typically not freed in the GC cycle */
                    /* as they may be referenced from multiple places */
                    break;

                default:
                    break;
            }

            free(val);
            gc->total_freed++;
        }
    }

    gc->object_count = write_idx;
}

/**
 * gc_collect - Full GC cycle: mark + sweep
 *
 * Main entry point for garbage collection.
 * Performs complete Mark-and-Sweep algorithm.
 */
void gc_collect(GC *gc, Value **roots, int root_count,
                void **env_chain, int env_count) {
    if (!gc) {
        return;
    }

    /* Phase 1: Mark reachable objects from roots */
    gc_mark_roots(gc, roots, root_count, env_chain, env_count);

    /* Phase 2: Sweep unreachable objects */
    gc_sweep(gc);

    /* Update statistics */
    gc->gc_count++;
}

/* ============================================================================
 * Statistics and Diagnostics
 * ============================================================================ */

/**
 * gc_print_stats - Print GC statistics
 */
void gc_print_stats(GC *gc) {
    if (!gc) {
        printf("GC is NULL\n");
        return;
    }

    int estimated_bytes = gc->object_count * (sizeof(Value) + 64);  /* rough est */

    printf("=== GC Statistics ===\n");
    printf("Objects tracked:    %d / %d\n", gc->object_count, gc->object_capacity);
    printf("GC threshold:       %d\n", gc->gc_threshold);
    printf("Collections run:    %d\n", gc->gc_count);
    printf("Total freed:        %d\n", gc->total_freed);
    printf("Est. memory:        %d bytes\n", estimated_bytes);
    printf("======================\n");
}

/**
 * gc_get_object_count - Get current object count
 */
int gc_get_object_count(GC *gc) {
    return gc ? gc->object_count : 0;
}

/**
 * gc_get_collection_count - Get lifetime collection count
 */
int gc_get_collection_count(GC *gc) {
    return gc ? gc->gc_count : 0;
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * gc_is_heap_value - Check if type requires GC tracking
 */
int gc_is_heap_value(ValType type) {
    switch (type) {
        case VAL_STRING:
        case VAL_ARRAY:
        case VAL_OBJECT:
        case VAL_FUNCTION:
        case VAL_CLOSURE:  /* NEW: Closures are heap-allocated */
            return 1;
        default:
            return 0;
    }
}

/**
 * gc_value_type_name - Get type name string
 */
const char *gc_value_type_name(ValType type) {
    switch (type) {
        case VAL_NULL:      return "NULL";
        case VAL_BOOL:      return "BOOL";
        case VAL_NUMBER:    return "NUMBER";
        case VAL_STRING:    return "STRING";
        case VAL_ARRAY:     return "ARRAY";
        case VAL_OBJECT:    return "OBJECT";
        case VAL_FUNCTION:  return "FUNCTION";
        case VAL_CLOSURE:   return "CLOSURE";  /* NEW: Closure type name */
        case VAL_NATIVE_FN: return "NATIVE_FN";
        default:            return "UNKNOWN";
    }
}

/**
 * gc_value_to_string - Convert value to string representation
 */
const char *gc_value_to_string(Value *val, char *buf, size_t buflen) {
    if (!buf || buflen == 0) {
        return NULL;
    }

    if (!val) {
        snprintf(buf, buflen, "null");
        return buf;
    }

    switch (val->type) {
        case VAL_NULL:
            snprintf(buf, buflen, "null");
            break;

        case VAL_BOOL:
            snprintf(buf, buflen, "%s", val->bool_val ? "true" : "false");
            break;

        case VAL_NUMBER:
            snprintf(buf, buflen, "%.14g", val->num_val);
            break;

        case VAL_STRING:
            snprintf(buf, buflen, "\"%s\"", val->str_val ? val->str_val : "");
            break;

        case VAL_ARRAY: {
            Vector *arr = (Vector *)val->array_val;
            int size = arr ? vector_size(arr) : 0;
            snprintf(buf, buflen, "[array of %d elements]", size);
            break;
        }

        case VAL_OBJECT:
            snprintf(buf, buflen, "[object]");
            break;

        case VAL_FUNCTION:
            snprintf(buf, buflen, "[Function]");
            break;

        case VAL_NATIVE_FN:
            snprintf(buf, buflen, "[native function]");
            break;

        default:
            snprintf(buf, buflen, "[unknown type]");
            break;
    }

    return buf;
}

/**
 * GC Implementation Stubs
 */

fl_gc_t* fl_gc_create(void) {
    fl_gc_t* gc = (fl_gc_t*)malloc(sizeof(fl_gc_t));
    if (!gc) return NULL;
    memset(gc, 0, sizeof(fl_gc_t));
    return gc;
}

void fl_gc_destroy(fl_gc_t* gc) {
    if (gc) {
        free(gc);
    }
}

void fl_gc_collect(fl_gc_t* gc) {
    /* Mark-and-sweep stub */
    if (!gc) return;
    /* TODO: Implement actual collection */
}
