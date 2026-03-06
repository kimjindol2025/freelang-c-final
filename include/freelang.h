/**
 * FreeLang C Runtime
 * Main header file with type definitions and declarations
 */

#ifndef FREELANG_H
#define FREELANG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "token.h"  /* Token type definitions */

/* Version info */
#define FL_VERSION_MAJOR 1
#define FL_VERSION_MINOR 0
#define FL_VERSION_PATCH 0

/* Type definitions */
typedef int64_t fl_int;
typedef double fl_float;
typedef bool fl_bool;
typedef char* fl_string;

/* Value types */
typedef enum {
    FL_TYPE_NULL,
    FL_TYPE_BOOL,
    FL_TYPE_INT,
    FL_TYPE_FLOAT,
    FL_TYPE_STRING,
    FL_TYPE_ARRAY,
    FL_TYPE_OBJECT,
    FL_TYPE_FUNCTION,
    FL_TYPE_CLOSURE,       /* NEW: Closure with captured environment */
    FL_TYPE_ERROR
} fl_type_t;

/* Value union */
typedef struct fl_value {
    fl_type_t type;
    union {
        fl_bool bool_val;
        fl_int int_val;
        fl_float float_val;
        fl_string string_val;
        struct fl_array* array_val;
        struct fl_object* object_val;
        struct fl_function* func_val;
        struct fl_closure* closure_val;    /* NEW: Closure with captured environment */
        struct fl_error* error_val;
    } data;
} fl_value_t;

/* Array structure */
typedef struct fl_array {
    fl_value_t* elements;
    size_t size;
    size_t capacity;
} fl_array_t;

/* Object structure (key-value map) */
typedef struct fl_object {
    char** keys;
    fl_value_t* values;
    size_t size;
    size_t capacity;
} fl_object_t;

/* Function structure */
typedef struct fl_function {
    char* name;
    uint8_t* bytecode;
    size_t bytecode_size;
    size_t arity;
    bool is_native;
    void* native_func;
    /* Constants pool for function bytecode */
    fl_value_t* consts;
    int const_count;
} fl_function_t;

/* Captured variable in closure environment */
typedef struct {
    char* name;             /* Variable name */
    fl_value_t value;       /* Captured value */
} fl_captured_var_t;

/* Closure structure - function with captured environment */
typedef struct fl_closure {
    fl_function_t* func;    /* The underlying function */

    /* Captured variables (lexical environment) */
    fl_captured_var_t* captured_vars;
    size_t captured_count;
    size_t captured_capacity;
} fl_closure_t;

/* Forward declare error type enum */
typedef enum {
    FL_ERR_NONE,
    FL_ERR_SYNTAX,
    FL_ERR_RUNTIME,
    FL_ERR_TYPE,
    FL_ERR_REFERENCE,
    FL_ERR_INDEX_OUT_OF_BOUNDS,
    FL_ERR_DIVIDE_BY_ZERO,
    FL_ERR_UNDEFINED_VARIABLE,
    FL_ERR_UNDEFINED_FUNCTION,
    FL_ERR_INVALID_ARGUMENT,
    FL_ERR_ASSERTION_FAILED,
    FL_ERR_IO,
    FL_ERR_CUSTOM
} fl_error_type_t;

/* Error structure */
typedef struct fl_error {
    fl_error_type_t type;
    char* message;
    char* filename;
    int line;
    int column;
    char* stack_trace;
} fl_error_t;

/* Token types - use TokenType from token.h */
typedef TokenType fl_token_type_t;  /* Compatibility alias */

/* Token structure - now matches token.h Token */
typedef Token fl_token_t;  /* Compatibility alias for fl_token_t = Token */

/* Opcode definitions */
typedef enum {
    /* Stack operations */
    FL_OP_PUSH_NULL,
    FL_OP_PUSH_BOOL,
    FL_OP_PUSH_INT,
    FL_OP_PUSH_FLOAT,
    FL_OP_PUSH_STRING,
    FL_OP_POP,
    FL_OP_DUP,

    /* Arithmetic */
    FL_OP_ADD,
    FL_OP_SUB,
    FL_OP_MUL,
    FL_OP_DIV,
    FL_OP_MOD,

    /* Comparison */
    FL_OP_EQ,
    FL_OP_NEQ,
    FL_OP_LT,
    FL_OP_LTE,
    FL_OP_GT,
    FL_OP_GTE,

    /* Logic */
    FL_OP_AND,
    FL_OP_OR,
    FL_OP_NOT,

    /* Control flow */
    FL_OP_JMP,
    FL_OP_JMPT,
    FL_OP_JMPF,
    FL_OP_CALL,
    FL_OP_RET,

    /* Variables */
    FL_OP_LOAD_LOCAL,
    FL_OP_STORE_LOCAL,
    FL_OP_LOAD_GLOBAL,
    FL_OP_STORE_GLOBAL,

    /* Arrays */
    FL_OP_ARRAY_NEW,
    FL_OP_ARRAY_GET,
    FL_OP_ARRAY_SET,
    FL_OP_ARRAY_LEN,

    /* Objects */
    FL_OP_OBJECT_NEW,
    FL_OP_OBJECT_GET,
    FL_OP_OBJECT_SET,

    /* Exceptions */
    FL_OP_TRY_START,
    FL_OP_CATCH_START,
    FL_OP_CATCH_END,
    FL_OP_THROW,

    /* Closures */
    FL_OP_MAKE_CLOSURE,     /* Create closure from function + captured vars */
    FL_OP_LOAD_UPVALUE,     /* Load captured variable from closure */
    FL_OP_STORE_UPVALUE,    /* Store captured variable in closure */

    /* Other */
    FL_OP_HALT,
    FL_OP_NOP
} fl_opcode_t;

/* Function declarations */

/* Lexer */
typedef struct fl_lexer fl_lexer_t;

fl_lexer_t* fl_lexer_create(const char* source);
void fl_lexer_destroy(fl_lexer_t* lexer);
fl_token_t fl_lexer_next(fl_lexer_t* lexer);

/* Parser */
typedef struct fl_parser fl_parser_t;
typedef struct fl_ast_node fl_ast_node_t;

fl_parser_t* fl_parser_create(fl_token_t* tokens, size_t token_count);
void fl_parser_destroy(fl_parser_t* parser);
fl_ast_node_t* fl_parser_parse(fl_parser_t* parser);

/* Virtual Machine */
typedef struct fl_vm fl_vm_t;

fl_vm_t* fl_vm_create(void);
void fl_vm_destroy(fl_vm_t* vm);
fl_value_t fl_vm_execute(fl_vm_t* vm, const void* chunk);  /* chunk is Chunk* */

/* Garbage Collector */
typedef struct {
    fl_value_t** objects;
    size_t count;
    size_t capacity;
    size_t threshold;
} fl_gc_t;

fl_gc_t* fl_gc_create(void);
void fl_gc_destroy(fl_gc_t* gc);
void fl_gc_collect(fl_gc_t* gc);

/* Runtime */
typedef struct fl_runtime fl_runtime_t;

fl_runtime_t* fl_runtime_create(void);
void fl_runtime_destroy(fl_runtime_t* runtime);
fl_value_t fl_runtime_eval(fl_runtime_t* runtime, const char* source);

/* Utilities */
const char* fl_type_name(fl_type_t type);
void fl_value_print(fl_value_t value);
void fl_value_free(fl_value_t value);

#endif /* FREELANG_H */
