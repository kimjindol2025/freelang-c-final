/**
 * FreeLang Virtual Machine Interface
 * Stack-based bytecode execution
 */

#ifndef FL_VM_H
#define FL_VM_H

#include "freelang.h"

#define FL_VM_STACK_SIZE 10000
#define FL_VM_FRAME_SIZE 256

typedef struct fl_call_frame {
    uint8_t* ip;
    fl_value_t* locals;
    size_t local_count;
    struct fl_closure* closure;  /* Current closure context (if any) */
} fl_call_frame_t;

typedef struct fl_vm {
    fl_value_t stack[FL_VM_STACK_SIZE];
    size_t stack_top;

    fl_call_frame_t frames[FL_VM_FRAME_SIZE];
    size_t frame_count;

    /* Local variables for current scope */
    fl_value_t locals[256];
    size_t local_count;

    fl_value_t globals;
    void* gc;

    /* Runtime reference for user-defined functions */
    void* runtime;  /* fl_runtime_t* */

    /* Exception handling */
    fl_value_t exception;      /* Current exception value */
    bool exception_active;     /* Exception currently being thrown */
    size_t exception_handler;  /* Instruction offset where to jump on exception */

    /* Closure context */
    struct fl_closure* current_closure;  /* Current closure (if executing closure) */
} fl_vm_t;

/* Create VM instance */
fl_vm_t* fl_vm_create(void);

/* Destroy VM */
void fl_vm_destroy(fl_vm_t* vm);

/* Execute bytecode with constants pool (chunk is void* to avoid circular includes) */
fl_value_t fl_vm_execute(fl_vm_t* vm, const void* chunk);

/* Stack operations */
void fl_vm_push(fl_vm_t* vm, fl_value_t value);
fl_value_t fl_vm_pop(fl_vm_t* vm);
fl_value_t fl_vm_peek(fl_vm_t* vm);

/* Global variables */
void fl_vm_set_global(fl_vm_t* vm, const char* name, fl_value_t value);
fl_value_t fl_vm_get_global(fl_vm_t* vm, const char* name);

/* Call functions */
fl_value_t fl_vm_call(fl_vm_t* vm, fl_value_t func, fl_value_t* args, size_t arg_count);

#endif /* FL_VM_H */
