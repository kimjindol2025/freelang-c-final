/**
 * FreeLang Runtime Environment
 * Manages execution context and global state
 */

#ifndef FL_RUNTIME_H
#define FL_RUNTIME_H

#include "freelang.h"

/* Forward declarations */
typedef struct fl_vm fl_vm_t;
/* fl_gc_t is defined in freelang.h */

/* Global variable store */
typedef struct {
    char **keys;            /* Variable names */
    fl_value_t *values;     /* Variable values */
    size_t count;
    size_t capacity;
} GlobalStore;

typedef struct fl_runtime {
    fl_vm_t* vm;
    fl_gc_t* gc;

    /* Global namespace */
    GlobalStore* globals;   /* Global variables */

    /* User-defined functions */
    struct {
        char *name;
        fl_function_t *func;
    } *user_functions;
    int user_func_count;
    int user_func_capacity;

    /* Builtin functions */
    void* builtins;         /* HashMap of builtin functions */

    /* Module system */
    void* modules;          /* HashMap of loaded modules */

    /* Error state */
    fl_error_t* last_error;
} fl_runtime_t;

/* Create runtime */
fl_runtime_t* fl_runtime_create(void);

/* Destroy runtime */
void fl_runtime_destroy(fl_runtime_t* runtime);

/* Evaluate source code */
fl_value_t fl_runtime_eval(fl_runtime_t* runtime, const char* source);

/* Execute file */
fl_value_t fl_runtime_exec_file(fl_runtime_t* runtime, const char* filename);

/* Set global variable */
void fl_runtime_set_global(fl_runtime_t* runtime, const char* name, fl_value_t value);

/* Get global variable */
fl_value_t fl_runtime_get_global(fl_runtime_t* runtime, const char* name);

/* Register builtin function */
void fl_runtime_register_builtin(fl_runtime_t* runtime, const char* name,
                                 fl_value_t (*func)(fl_value_t*, size_t));

/* Get/set error state */
fl_error_t* fl_runtime_get_error(fl_runtime_t* runtime);
void fl_runtime_clear_error(fl_runtime_t* runtime);

/* Register/find user-defined functions */
void fl_runtime_register_function(fl_runtime_t* runtime, const char* name,
                                   fl_function_t* func);
fl_function_t* fl_runtime_find_function(fl_runtime_t* runtime, const char* name);

#endif /* FL_RUNTIME_H */
