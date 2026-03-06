/**
 * FreeLang Compiler Interface
 * AST to bytecode compilation
 */

#ifndef FL_COMPILER_H
#define FL_COMPILER_H

#include "freelang.h"
#include "ast.h"

/* Forward declaration */
typedef struct fl_ast_node fl_ast_node_t;

/* Bytecode chunk - represents compiled code */
typedef struct {
    uint8_t *code;
    size_t code_len;
    size_t code_capacity;

    /* Constants pool (strings, numbers, etc) */
    fl_value_t *consts;
    size_t const_len;
    size_t const_capacity;

    /* Local variables count */
    int locals_count;

    /* Line number mapping for debugging */
    int *line_map;
    size_t line_map_capacity;
} Chunk;

/* Forward declaration */
typedef struct fl_function fl_function_t;

/* Captured variable metadata */
typedef struct {
    char name[256];         /* Variable name */
    int scope_level;        /* At which scope level it was defined */
    int local_idx;          /* Index in the parent scope */
} CapturedVarInfo;

/* Compiler state */
typedef struct {
    Chunk *chunk;

    /* Break/continue tracking for loops */
    int break_addrs[100];
    int break_count;
    int continue_addrs[100];
    int continue_count;

    /* Local variables tracking */
    char local_names[256][256];
    int local_count;

    /* Scope level tracking for closure capture analysis */
    int scope_level;        /* 0 = module, 1+ = nested functions */

    /* Captured variables list for current function */
    CapturedVarInfo captured_vars[256];
    int captured_count;

    /* Functions table - stores user-defined functions */
    fl_function_t *functions[256];
    int function_count;
} Compiler;

/* ============================================================
   Compiler API
   ============================================================ */

/**
 * Create a new compiler instance
 */
Compiler* compiler_new(void);

/**
 * Free compiler resources
 */
void compiler_free(Compiler *c);

/**
 * Compile entire program AST to bytecode
 */
Chunk* compile_program(Compiler *c, fl_ast_node_t *node);

/**
 * Compile a single expression
 */
Chunk* compile_expr(Compiler *c, fl_ast_node_t *node);

/**
 * Compile a statement
 */
Chunk* compile_stmt(Compiler *c, fl_ast_node_t *node);

/* ============================================================
   Chunk Management
   ============================================================ */

/**
 * Create empty chunk
 */
Chunk* chunk_new(void);

/**
 * Free chunk resources
 */
void chunk_free(Chunk *chunk);

/**
 * Emit bytecode instruction
 */
void chunk_emit_opcode(Chunk *chunk, fl_opcode_t opcode);

/**
 * Emit integer constant
 */
int chunk_emit_int(Chunk *chunk, fl_int value);

/**
 * Emit float constant
 */
int chunk_emit_float(Chunk *chunk, fl_float value);

/**
 * Emit string constant
 */
int chunk_emit_string(Chunk *chunk, const char *value);

/**
 * Add local variable
 */
int chunk_add_local(Chunk *chunk, const char *name);

/**
 * Get local variable index
 */
int chunk_get_local(Chunk *chunk, const char *name);

/**
 * Patch jump address (for if/while jumps)
 */
void chunk_patch_jump(Chunk *chunk, int addr);

/**
 * Get current bytecode offset
 */
size_t chunk_offset(Chunk *chunk);

/**
 * Emit 4-byte address operand
 */
void chunk_emit_addr(Chunk *chunk, uint32_t addr);

#endif /* FL_COMPILER_H */
