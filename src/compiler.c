/**
 * FreeLang Compiler Implementation
 * AST to Bytecode Compilation
 * Phase 3-A: Simple Bytecode Generator
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../include/compiler.h"
#include "../include/ast.h"
#include "../include/freelang.h"
#include "../include/logger.h"

/* ============================================================
   @log_level 어노테이션 처리
   소스 파일 상단에 "@log_level(debug|info|warn|error|off)" 선언 시
   컴파일러가 해당 레벨 이하의 log_*() 호출을 NOP으로 대체
   ============================================================ */

static void compiler_apply_log_level(const char* level_str) {
    if (!level_str) return;
    if      (strcmp(level_str, "debug") == 0) fl_compile_log_level = FL_LOG_DEBUG;
    else if (strcmp(level_str, "info")  == 0) fl_compile_log_level = FL_LOG_INFO;
    else if (strcmp(level_str, "warn")  == 0) fl_compile_log_level = FL_LOG_WARN;
    else if (strcmp(level_str, "error") == 0) fl_compile_log_level = FL_LOG_ERROR;
    else if (strcmp(level_str, "off")   == 0) fl_compile_log_level = FL_LOG_OFF;
    fprintf(stderr, "[Compiler] @log_level(\"%s\") → 컴파일 임계값 설정\n", level_str);
}

/* log_*() 호출이 컴파일 레벨에 의해 제거되어야 하는지 확인 */
static bool compiler_log_call_should_drop(const char* func_name) {
    if (!func_name) return false;
    fl_log_level_t call_level = FL_LOG_OFF;
    if      (strcmp(func_name, "log_debug") == 0) call_level = FL_LOG_DEBUG;
    else if (strcmp(func_name, "log_info")  == 0) call_level = FL_LOG_INFO;
    else if (strcmp(func_name, "log_warn")  == 0) call_level = FL_LOG_WARN;
    else if (strcmp(func_name, "log_error") == 0) call_level = FL_LOG_ERROR;
    else return false;  /* 로거 함수가 아님 */

    /* call_level < compile_level 이면 제거 */
    return (int)call_level < (int)fl_compile_log_level;
}

/* ============================================================
   Chunk Management
   ============================================================ */

Chunk* chunk_new(void) {
    Chunk* chunk = (Chunk*)malloc(sizeof(Chunk));
    if (!chunk) return NULL;

    chunk->code_capacity = 256;
    chunk->code = (uint8_t*)malloc(chunk->code_capacity);
    if (!chunk->code) {
        free(chunk);
        return NULL;
    }
    chunk->code_len = 0;

    chunk->const_capacity = 32;
    chunk->consts = (fl_value_t*)malloc(chunk->const_capacity * sizeof(fl_value_t));
    if (!chunk->consts) {
        free(chunk->code);
        free(chunk);
        return NULL;
    }
    chunk->const_len = 0;

    chunk->locals_count = 0;
    chunk->line_map = NULL;
    chunk->line_map_capacity = 0;

    return chunk;
}

void chunk_free(Chunk* chunk) {
    if (!chunk) return;
    if (chunk->code) free(chunk->code);
    if (chunk->consts) free(chunk->consts);
    if (chunk->line_map) free(chunk->line_map);
    free(chunk);
}

void chunk_emit_opcode(Chunk* chunk, fl_opcode_t opcode) {
    if (!chunk) return;

    if (chunk->code_len >= chunk->code_capacity) {
        chunk->code_capacity *= 2;
        uint8_t* new_code = (uint8_t*)realloc(chunk->code, chunk->code_capacity);
        if (!new_code) return;
        chunk->code = new_code;
    }

    chunk->code[chunk->code_len++] = (uint8_t)opcode;
}

int chunk_emit_int(Chunk* chunk, fl_int value) {
    if (!chunk) return -1;

    if (chunk->const_len >= chunk->const_capacity) {
        chunk->const_capacity *= 2;
        fl_value_t* new_consts = (fl_value_t*)realloc(chunk->consts, chunk->const_capacity * sizeof(fl_value_t));
        if (!new_consts) return -1;
        chunk->consts = new_consts;
    }

    int idx = chunk->const_len;
    chunk->consts[idx].type = FL_TYPE_INT;
    chunk->consts[idx].data.int_val = value;
    chunk->const_len++;

    return idx;
}

int chunk_emit_float(Chunk* chunk, fl_float value) {
    if (!chunk) return -1;

    if (chunk->const_len >= chunk->const_capacity) {
        chunk->const_capacity *= 2;
        fl_value_t* new_consts = (fl_value_t*)realloc(chunk->consts, chunk->const_capacity * sizeof(fl_value_t));
        if (!new_consts) return -1;
        chunk->consts = new_consts;
    }

    int idx = chunk->const_len;
    chunk->consts[idx].type = FL_TYPE_FLOAT;
    chunk->consts[idx].data.float_val = value;
    chunk->const_len++;

    return idx;
}

int chunk_emit_string(Chunk* chunk, const char* value) {
    if (!chunk || !value) return -1;

    if (chunk->const_len >= chunk->const_capacity) {
        chunk->const_capacity *= 2;
        fl_value_t* new_consts = (fl_value_t*)realloc(chunk->consts, chunk->const_capacity * sizeof(fl_value_t));
        if (!new_consts) return -1;
        chunk->consts = new_consts;
    }

    int idx = chunk->const_len;
    chunk->consts[idx].type = FL_TYPE_STRING;
    chunk->consts[idx].data.string_val = (char*)malloc(strlen(value) + 1);
    if (!chunk->consts[idx].data.string_val) return -1;
    strcpy(chunk->consts[idx].data.string_val, value);
    chunk->const_len++;

    return idx;
}

int chunk_add_local(Chunk* chunk, const char* name) {
    if (!chunk) return -1;
    (void)name;  /* unused */
    return chunk->locals_count++;
}

int chunk_get_local(Chunk* chunk, const char* name) {
    if (!chunk) return -1;
    (void)name;  /* unused */
    return 0;
}

/* Find a local variable by name in the compiler's local_names array */
static int compiler_find_local(Compiler* c, const char* name) {
    if (!c || !name) return -1;
    for (int i = 0; i < c->local_count; i++) {
        if (strcmp(c->local_names[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

/* Find a user-defined function by name */
static int compiler_find_function(Compiler* c, const char* name) {
    if (!c || !name) return -1;
    for (int i = 0; i < c->function_count; i++) {
        if (c->functions[i] && strcmp(c->functions[i]->name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/* Add a local variable to the compiler's tracking */
static int compiler_add_local(Compiler* c, const char* name) {
    if (!c || !name || c->local_count >= 256) return -1;
    strncpy(c->local_names[c->local_count], name, 255);
    c->local_names[c->local_count][255] = '\0';
    return c->local_count++;
}

void chunk_patch_jump(Chunk* chunk, int addr) {
    if (!chunk || addr < 0) return;
}

size_t chunk_offset(Chunk* chunk) {
    if (!chunk) return 0;
    return chunk->code_len;
}

void chunk_emit_addr(Chunk* chunk, uint32_t addr) {
    if (!chunk) return;
    if (chunk->code_len + 4 >= chunk->code_capacity) {
        chunk->code_capacity *= 2;
        uint8_t* new_code = (uint8_t*)realloc(chunk->code, chunk->code_capacity);
        if (!new_code) return;
        chunk->code = new_code;
    }

    chunk->code[chunk->code_len++] = (addr >> 24) & 0xFF;
    chunk->code[chunk->code_len++] = (addr >> 16) & 0xFF;
    chunk->code[chunk->code_len++] = (addr >> 8) & 0xFF;
    chunk->code[chunk->code_len++] = addr & 0xFF;
}

/* ============================================================
   Compiler State
   ============================================================ */

Compiler* compiler_new(void) {
    Compiler* compiler = (Compiler*)malloc(sizeof(Compiler));
    if (!compiler) return NULL;

    compiler->chunk = chunk_new();
    if (!compiler->chunk) {
        free(compiler);
        return NULL;
    }

    compiler->break_count = 0;
    compiler->continue_count = 0;
    compiler->local_count = 0;
    memset(compiler->local_names, 0, sizeof(compiler->local_names));

    /* Initialize scope tracking */
    compiler->scope_level = 0;
    compiler->captured_count = 0;
    memset(compiler->captured_vars, 0, sizeof(compiler->captured_vars));

    /* Initialize function table */
    compiler->function_count = 0;
    memset(compiler->functions, 0, sizeof(compiler->functions));

    return compiler;
}

void compiler_free(Compiler* c) {
    if (!c) return;
    if (c->chunk) chunk_free(c->chunk);
    free(c);
}

/* ============================================================
   Variable Capture Analysis (for closures)
   ============================================================ */

/**
 * Check if a variable is in the current scope (local variables)
 * Returns index if found, -1 if not found
 */
static int is_local_var(Compiler* c, const char* name) {
    for (int i = 0; i < c->local_count; i++) {
        if (strcmp(c->local_names[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * Check if a variable is already captured
 * Returns index if found, -1 if not found
 */
static int is_captured_var(Compiler* c, const char* name) {
    for (int i = 0; i < c->captured_count; i++) {
        if (strcmp(c->captured_vars[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * Add a variable to the captured list (if not already there)
 * Returns index in captured_vars array
 */
static int add_captured_var(Compiler* c, const char* name, int scope_level, int local_idx) {
    /* Check if already captured */
    int existing = is_captured_var(c, name);
    if (existing >= 0) {
        return existing;
    }

    /* Add new captured variable */
    if (c->captured_count >= 256) {
        fprintf(stderr, "[ERROR] Too many captured variables\n");
        return -1;
    }

    int idx = c->captured_count;
    strncpy(c->captured_vars[idx].name, name, 255);
    c->captured_vars[idx].name[255] = '\0';
    c->captured_vars[idx].scope_level = scope_level;
    c->captured_vars[idx].local_idx = local_idx;
    c->captured_count++;

    fprintf(stderr, "[CAPTURE] Variable '%s' (scope_level=%d, local_idx=%d) captured\n",
            name, scope_level, local_idx);

    return idx;
}

/**
 * Clear captured variables list for a new scope
 */
static void clear_captured_vars(Compiler* c) {
    c->captured_count = 0;
    memset(c->captured_vars, 0, sizeof(c->captured_vars));
}

/**
 * Analyze AST node for variable references (used in closures)
 * Recursively finds all variable identifiers and checks if they should be captured
 */
static void analyze_captures(Compiler* c, fl_ast_node_t* node, int parent_scope_level) {
    if (!node) return;

    switch (node->type) {
        case NODE_IDENT: {
            const char* name = node->data.ident.name;
            /* Check if this identifier is a local variable in parent scope */
            /* For now, we'll mark all identifiers as potentially captured */
            /* In a real implementation, we'd check the outer scope */
            fprintf(stderr, "[ANALYZE] Found identifier: %s\n", name);
            break;
        }

        case NODE_BINARY: {
            analyze_captures(c, node->data.binary.left, parent_scope_level);
            analyze_captures(c, node->data.binary.right, parent_scope_level);
            break;
        }

        case NODE_UNARY: {
            analyze_captures(c, node->data.unary.operand, parent_scope_level);
            break;
        }

        case NODE_LOGICAL: {
            analyze_captures(c, node->data.logical.left, parent_scope_level);
            analyze_captures(c, node->data.logical.right, parent_scope_level);
            break;
        }

        case NODE_ASSIGN: {
            analyze_captures(c, node->data.assign.target, parent_scope_level);
            analyze_captures(c, node->data.assign.value, parent_scope_level);
            break;
        }

        case NODE_CALL: {
            analyze_captures(c, node->data.call.callee, parent_scope_level);
            for (int i = 0; i < node->data.call.arg_count; i++) {
                analyze_captures(c, node->data.call.arguments[i], parent_scope_level);
            }
            break;
        }

        case NODE_MEMBER: {
            analyze_captures(c, node->data.member.object, parent_scope_level);
            analyze_captures(c, node->data.member.property, parent_scope_level);
            break;
        }

        case NODE_ARRAY: {
            for (int i = 0; i < node->data.array.element_count; i++) {
                analyze_captures(c, node->data.array.elements[i], parent_scope_level);
            }
            break;
        }

        case NODE_OBJECT: {
            for (int i = 0; i < node->data.object.property_count; i++) {
                analyze_captures(c, node->data.object.properties[i], parent_scope_level);
            }
            break;
        }

        case NODE_PROPERTY: {
            analyze_captures(c, node->data.property.value, parent_scope_level);
            break;
        }

        case NODE_RETURN: {
            if (node->data.return_stmt.value) {
                analyze_captures(c, node->data.return_stmt.value, parent_scope_level);
            }
            break;
        }

        case NODE_IF: {
            analyze_captures(c, node->data.if_stmt.test, parent_scope_level);
            analyze_captures(c, node->data.if_stmt.consequent, parent_scope_level);
            if (node->data.if_stmt.alternate) {
                analyze_captures(c, node->data.if_stmt.alternate, parent_scope_level);
            }
            break;
        }

        case NODE_WHILE: {
            analyze_captures(c, node->data.while_stmt.test, parent_scope_level);
            analyze_captures(c, node->data.while_stmt.body, parent_scope_level);
            break;
        }

        case NODE_FOR: {
            if (node->data.for_stmt.init) {
                analyze_captures(c, node->data.for_stmt.init, parent_scope_level);
            }
            if (node->data.for_stmt.test) {
                analyze_captures(c, node->data.for_stmt.test, parent_scope_level);
            }
            if (node->data.for_stmt.update) {
                analyze_captures(c, node->data.for_stmt.update, parent_scope_level);
            }
            analyze_captures(c, node->data.for_stmt.body, parent_scope_level);
            break;
        }

        case NODE_FOR_IN: {
            analyze_captures(c, node->data.for_in_stmt.iterable, parent_scope_level);
            analyze_captures(c, node->data.for_in_stmt.body, parent_scope_level);
            break;
        }

        case NODE_BLOCK: {
            for (int i = 0; i < node->data.block.stmt_count; i++) {
                analyze_captures(c, node->data.block.statements[i], parent_scope_level);
            }
            break;
        }

        case NODE_VAR_DECL: {
            if (node->data.var_decl.init) {
                analyze_captures(c, node->data.var_decl.init, parent_scope_level);
            }
            break;
        }

        case NODE_TERNARY: {
            analyze_captures(c, node->data.ternary.test, parent_scope_level);
            analyze_captures(c, node->data.ternary.consequent, parent_scope_level);
            analyze_captures(c, node->data.ternary.alternate, parent_scope_level);
            break;
        }

        case NODE_TRY: {
            analyze_captures(c, node->data.try_stmt.body, parent_scope_level);
            if (node->data.try_stmt.catch_clause) {
                analyze_captures(c, node->data.try_stmt.catch_clause, parent_scope_level);
            }
            if (node->data.try_stmt.finally_clause) {
                analyze_captures(c, node->data.try_stmt.finally_clause, parent_scope_level);
            }
            break;
        }

        case NODE_CATCH_CLAUSE: {
            analyze_captures(c, node->data.catch_clause.handler, parent_scope_level);
            break;
        }

        case NODE_FINALLY_CLAUSE: {
            analyze_captures(c, node->data.finally_clause.handler, parent_scope_level);
            break;
        }

        case NODE_THROW: {
            analyze_captures(c, node->data.throw_stmt.argument, parent_scope_level);
            break;
        }

        /* FN_EXPR and ARROW_FN create new scopes - don't analyze deeper */
        case NODE_FN_EXPR:
        case NODE_ARROW_FN:
        case NODE_FN_DECL:
            /* These are handled separately with their own scope */
            break;

        default:
            /* Other node types don't contain variable references */
            break;
    }
}

/* ============================================================
   Compilation Functions (Recursive AST Traversal)
   ============================================================ */

static void compile_node(Compiler* c, fl_ast_node_t* node);

/* @log_level 어노테이션 스캔: 소스 텍스트에서 직접 파싱
 * 형식: "@log_level(debug)" 또는 "@log_level(\"info\")"
 * AST 레벨이 아닌 소스 스캔으로 처리 (컴파일 전 최우선 적용) */
void scan_log_level_annotation(const char* source) {
    if (!source) return;
    const char* p = strstr(source, "@log_level");
    if (!p) return;
    p += strlen("@log_level");
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '(') return;
    p++;
    while (*p == ' ' || *p == '"' || *p == '\'') p++;
    char level[16] = {0};
    size_t i = 0;
    while (*p && *p != ')' && *p != '"' && *p != '\'' && i < 15) {
        level[i++] = *p++;
    }
    level[i] = '\0';
    /* 공백 제거 */
    while (i > 0 && (level[i-1] == ' ' || level[i-1] == '\t')) level[--i] = '\0';
    if (i > 0) compiler_apply_log_level(level);
}

Chunk* compile_program(Compiler* c, fl_ast_node_t* node) {
    if (!c || !node || node->type != NODE_PROGRAM) {
        return NULL;
    }

    /* Pass 1: Collect all function declarations */
    fprintf(stderr, "[COMPILER] Pass 1: Collecting function declarations\n");
    for (int i = 0; i < node->data.program.item_count; i++) {
        if (node->data.program.items[i] &&
            node->data.program.items[i]->type == NODE_FN_DECL) {
            compile_node(c, node->data.program.items[i]);
        }
    }
    fprintf(stderr, "[COMPILER] Pass 1 complete: %d functions collected\n",
            c->function_count);

    /* Pass 2: Compile all other statements */
    fprintf(stderr, "[COMPILER] Pass 2: Compiling program statements\n");
    for (int i = 0; i < node->data.program.item_count; i++) {
        if (node->data.program.items[i] &&
            node->data.program.items[i]->type != NODE_FN_DECL) {
            compile_node(c, node->data.program.items[i]);
        }
    }

    chunk_emit_opcode(c->chunk, FL_OP_HALT);
    return c->chunk;
}

Chunk* compile_expr(Compiler* c, fl_ast_node_t* node) {
    if (!c || !node) return c->chunk;
    compile_node(c, node);
    return c->chunk;
}

Chunk* compile_stmt(Compiler* c, fl_ast_node_t* node) {
    if (!c || !node) return c->chunk;
    compile_node(c, node);
    return c->chunk;
}

static void compile_node(Compiler* c, fl_ast_node_t* node) {
    if (!c || !node) return;

    switch (node->type) {
        case NODE_LITERAL: {
            if (node->data.literal.is_null) {
                chunk_emit_opcode(c->chunk, FL_OP_PUSH_NULL);
            } else if (node->data.literal.is_bool) {
                chunk_emit_opcode(c->chunk, FL_OP_PUSH_BOOL);
                chunk_emit_addr(c->chunk, node->data.literal.bool_val ? 1 : 0);
            } else if (node->data.literal.str) {
                int idx = chunk_emit_string(c->chunk, node->data.literal.str);
                chunk_emit_opcode(c->chunk, FL_OP_PUSH_STRING);
                chunk_emit_addr(c->chunk, (uint32_t)idx);
            } else {
                if (node->data.literal.num == (double)(int64_t)node->data.literal.num) {
                    int idx = chunk_emit_int(c->chunk, (int64_t)node->data.literal.num);
                    chunk_emit_opcode(c->chunk, FL_OP_PUSH_INT);
                    chunk_emit_addr(c->chunk, (uint32_t)idx);
                } else {
                    int idx = chunk_emit_float(c->chunk, node->data.literal.num);
                    chunk_emit_opcode(c->chunk, FL_OP_PUSH_FLOAT);
                    chunk_emit_addr(c->chunk, (uint32_t)idx);
                }
            }
            break;
        }

        case NODE_IDENT: {
            /* Check if this is a local variable */
            int local_idx = compiler_find_local(c, node->data.ident.name);
            if (local_idx >= 0) {
                /* It's a local variable - load it */
                chunk_emit_opcode(c->chunk, FL_OP_LOAD_LOCAL);
                chunk_emit_addr(c->chunk, (uint32_t)local_idx);
            } else {
                /* Check if this is a user-defined function */
                int fn_idx = compiler_find_function(c, node->data.ident.name);
                if (fn_idx >= 0) {
                    /* It's a user-defined function - emit special opcode with function index */
                    /* For now, push as string (will be resolved at runtime) */
                    int str_idx = chunk_emit_string(c->chunk, node->data.ident.name);
                    chunk_emit_opcode(c->chunk, FL_OP_PUSH_STRING);
                    chunk_emit_addr(c->chunk, (uint32_t)str_idx);
                } else {
                    /* Unknown identifier - assume it's a builtin function or variable */
                    int str_idx = chunk_emit_string(c->chunk, node->data.ident.name);
                    chunk_emit_opcode(c->chunk, FL_OP_PUSH_STRING);
                    chunk_emit_addr(c->chunk, (uint32_t)str_idx);
                }
            }
            break;
        }

        case NODE_BINARY: {
            compile_node(c, node->data.binary.left);
            compile_node(c, node->data.binary.right);

            const char* op = node->data.binary.op;
            if (strcmp(op, "+") == 0) chunk_emit_opcode(c->chunk, FL_OP_ADD);
            else if (strcmp(op, "-") == 0) chunk_emit_opcode(c->chunk, FL_OP_SUB);
            else if (strcmp(op, "*") == 0) chunk_emit_opcode(c->chunk, FL_OP_MUL);
            else if (strcmp(op, "/") == 0) chunk_emit_opcode(c->chunk, FL_OP_DIV);
            else if (strcmp(op, "%") == 0) chunk_emit_opcode(c->chunk, FL_OP_MOD);
            else if (strcmp(op, "==") == 0) chunk_emit_opcode(c->chunk, FL_OP_EQ);
            else if (strcmp(op, "!=") == 0) chunk_emit_opcode(c->chunk, FL_OP_NEQ);
            else if (strcmp(op, "<") == 0) chunk_emit_opcode(c->chunk, FL_OP_LT);
            else if (strcmp(op, "<=") == 0) chunk_emit_opcode(c->chunk, FL_OP_LTE);
            else if (strcmp(op, ">") == 0) chunk_emit_opcode(c->chunk, FL_OP_GT);
            else if (strcmp(op, ">=") == 0) chunk_emit_opcode(c->chunk, FL_OP_GTE);
            break;
        }

        case NODE_UNARY: {
            compile_node(c, node->data.unary.operand);
            const char* op = node->data.unary.op;
            if (strcmp(op, "!") == 0) {
                chunk_emit_opcode(c->chunk, FL_OP_NOT);
            }
            break;
        }

        case NODE_LOGICAL: {
            /* Compile logical operators (&&, ||) */
            compile_node(c, node->data.logical.left);
            compile_node(c, node->data.logical.right);

            const char* op = node->data.logical.op;
            if (strcmp(op, "&&") == 0) {
                chunk_emit_opcode(c->chunk, FL_OP_AND);
            } else if (strcmp(op, "||") == 0) {
                chunk_emit_opcode(c->chunk, FL_OP_OR);
            }
            break;
        }

        case NODE_CALL: {
            /* @log_level 컴파일 시 필터링:
             * log_debug/info/warn/error 호출이 compile_log_level 미만이면
             * 인자 컴파일 없이 NOP 1개만 emit → 런타임 오버헤드 0 */
            const char* callee_name = NULL;
            if (node->data.call.callee &&
                node->data.call.callee->type == NODE_IDENT) {
                callee_name = node->data.call.callee->data.ident.name;
            }
            if (callee_name && compiler_log_call_should_drop(callee_name)) {
                chunk_emit_opcode(c->chunk, FL_OP_NOP);
                break;
            }

            /* Compile arguments first (standard calling convention) */
            for (int i = 0; i < node->data.call.arg_count; i++) {
                compile_node(c, node->data.call.arguments[i]);
            }
            /* Compile function name (callee) - should be on top of stack */
            compile_node(c, node->data.call.callee);
            /* Emit call instruction */
            chunk_emit_opcode(c->chunk, FL_OP_CALL);
            chunk_emit_addr(c->chunk, (uint32_t)node->data.call.arg_count);
            break;
        }

        case NODE_VAR_DECL: {
            if (node->data.var_decl.init) {
                compile_node(c, node->data.var_decl.init);
            } else {
                chunk_emit_opcode(c->chunk, FL_OP_PUSH_NULL);
            }
            /* Register the local variable and get its index */
            int local_idx = compiler_add_local(c, node->data.var_decl.name);
            if (local_idx < 0) return;  /* Error: too many locals */
            chunk_emit_opcode(c->chunk, FL_OP_STORE_LOCAL);
            chunk_emit_addr(c->chunk, (uint32_t)local_idx);  /* Use 4-byte operand */
            break;
        }

        /* Phase 4: NODE_REACTIVE_DECL - reactive let/const/var */
        case NODE_REACTIVE_DECL: {
            // 1. Compile initialization value
            if (node->data.reactive_decl.init) {
                compile_node(c, node->data.reactive_decl.init);
            } else {
                chunk_emit_opcode(c->chunk, FL_OP_PUSH_NULL);
            }

            // 2. Wrap with reactive behavior
            chunk_emit_opcode(c->chunk, FL_OP_MAKE_REACTIVE);

            // 3. Handle decorator (@watch, @transaction)
            if (strlen(node->data.reactive_decl.decorator) > 0) {
                if (strcmp(node->data.reactive_decl.decorator, "watch") == 0) {
                    chunk_emit_opcode(c->chunk, FL_OP_WATCH_FIELD);
                    // Emit field name as string constant
                    int field_const = chunk_emit_string(c->chunk, node->data.reactive_decl.name);
                    chunk_emit_addr(c->chunk, (uint32_t)field_const);
                } else if (strcmp(node->data.reactive_decl.decorator, "transaction") == 0) {
                    // Transaction decorator: runtime will handle
                    // (Can be applied at runtime instead of compile-time)
                }
            }

            // 4. Register local variable
            int local_idx = compiler_add_local(c, node->data.reactive_decl.name);
            if (local_idx < 0) return;  /* Error: too many locals */
            chunk_emit_opcode(c->chunk, FL_OP_STORE_LOCAL);
            chunk_emit_addr(c->chunk, (uint32_t)local_idx);
            break;
        }

        case NODE_ASSIGN: {
            if (node->data.assign.target->type == NODE_IDENT) {
                /* Simple variable assignment: x = val */
                compile_node(c, node->data.assign.value);
                const char* var_name = node->data.assign.target->data.ident.name;
                int local_idx = compiler_find_local(c, var_name);

                if (local_idx >= 0) {
                    /* It's an existing local variable */
                    chunk_emit_opcode(c->chunk, FL_OP_STORE_LOCAL);
                    chunk_emit_addr(c->chunk, (uint32_t)local_idx);
                } else {
                    /* New local variable - register it */
                    local_idx = compiler_add_local(c, var_name);
                    if (local_idx < 0) return;
                    chunk_emit_opcode(c->chunk, FL_OP_STORE_LOCAL);
                    chunk_emit_addr(c->chunk, (uint32_t)local_idx);
                }
            } else if (node->data.assign.target->type == NODE_MEMBER) {
                /* Array/object member assignment: arr[idx] = val */
                fl_ast_node_t *target = node->data.assign.target;
                if (target->data.member.is_computed) {
                    /* arr[idx] = val
                     * Stack needs: obj, idx, val (bottom to top)
                     */
                    compile_node(c, target->data.member.object);    /* obj on stack */
                    compile_node(c, target->data.member.property);  /* idx on stack */
                    compile_node(c, node->data.assign.value);       /* val on stack */
                    chunk_emit_opcode(c->chunk, FL_OP_ARRAY_SET);
                }
            }
            break;
        }

        case NODE_RETURN: {
            if (node->data.return_stmt.value) {
                compile_node(c, node->data.return_stmt.value);
            } else {
                chunk_emit_opcode(c->chunk, FL_OP_PUSH_NULL);
            }
            chunk_emit_opcode(c->chunk, FL_OP_RET);
            break;
        }

        case NODE_BLOCK: {
            for (int i = 0; i < node->data.block.stmt_count; i++) {
                compile_node(c, node->data.block.statements[i]);
            }
            break;
        }

        case NODE_IF: {
            /* Compile test condition */
            compile_node(c, node->data.if_stmt.test);

            /* Emit JMPF to skip consequent if condition is false */
            size_t jmpf_addr = chunk_offset(c->chunk);
            chunk_emit_opcode(c->chunk, FL_OP_JMPF);
            chunk_emit_addr(c->chunk, 0);  /* placeholder for jump address */

            /* Compile consequent block */
            compile_node(c, node->data.if_stmt.consequent);

            /* If there's an alternate, emit JMP to skip it */
            if (node->data.if_stmt.alternate) {
                size_t jmp_addr = chunk_offset(c->chunk);
                chunk_emit_opcode(c->chunk, FL_OP_JMP);
                chunk_emit_addr(c->chunk, 0);  /* placeholder for jump address */

                /* Patch JMPF to jump to alternate */
                uint32_t alternate_offset = (uint32_t)chunk_offset(c->chunk);
                c->chunk->code[jmpf_addr + 1] = (alternate_offset >> 24) & 0xFF;
                c->chunk->code[jmpf_addr + 2] = (alternate_offset >> 16) & 0xFF;
                c->chunk->code[jmpf_addr + 3] = (alternate_offset >> 8) & 0xFF;
                c->chunk->code[jmpf_addr + 4] = alternate_offset & 0xFF;

                /* Compile alternate */
                compile_node(c, node->data.if_stmt.alternate);

                /* Patch JMP to jump past alternate */
                uint32_t end_offset = (uint32_t)chunk_offset(c->chunk);
                c->chunk->code[jmp_addr + 1] = (end_offset >> 24) & 0xFF;
                c->chunk->code[jmp_addr + 2] = (end_offset >> 16) & 0xFF;
                c->chunk->code[jmp_addr + 3] = (end_offset >> 8) & 0xFF;
                c->chunk->code[jmp_addr + 4] = end_offset & 0xFF;
            } else {
                /* Patch JMPF to skip consequent */
                uint32_t end_offset = (uint32_t)chunk_offset(c->chunk);
                c->chunk->code[jmpf_addr + 1] = (end_offset >> 24) & 0xFF;
                c->chunk->code[jmpf_addr + 2] = (end_offset >> 16) & 0xFF;
                c->chunk->code[jmpf_addr + 3] = (end_offset >> 8) & 0xFF;
                c->chunk->code[jmpf_addr + 4] = end_offset & 0xFF;
            }
            break;
        }

        case NODE_WHILE: {
            /* Loop start position */
            size_t loop_start = chunk_offset(c->chunk);

            /* Compile test condition */
            compile_node(c, node->data.while_stmt.test);

            /* Emit JMPF to exit loop */
            size_t jmpf_addr = chunk_offset(c->chunk);
            chunk_emit_opcode(c->chunk, FL_OP_JMPF);
            chunk_emit_addr(c->chunk, 0);  /* placeholder for exit address */

            /* Compile body */
            compile_node(c, node->data.while_stmt.body);

            /* Emit JMP back to loop start */
            chunk_emit_opcode(c->chunk, FL_OP_JMP);
            chunk_emit_addr(c->chunk, (uint32_t)loop_start);

            /* Patch JMPF to jump past loop */
            uint32_t exit_offset = (uint32_t)chunk_offset(c->chunk);
            c->chunk->code[jmpf_addr + 1] = (exit_offset >> 24) & 0xFF;
            c->chunk->code[jmpf_addr + 2] = (exit_offset >> 16) & 0xFF;
            c->chunk->code[jmpf_addr + 3] = (exit_offset >> 8) & 0xFF;
            c->chunk->code[jmpf_addr + 4] = exit_offset & 0xFF;
            break;
        }

        case NODE_FOR: {
            /* Compile init expression (if present) */
            if (node->data.for_stmt.init) {
                compile_node(c, node->data.for_stmt.init);
            }

            /* Loop start position */
            size_t loop_start = chunk_offset(c->chunk);

            /* Compile test condition (if present) */
            if (node->data.for_stmt.test) {
                compile_node(c, node->data.for_stmt.test);

                /* Emit JMPF to exit loop */
                size_t jmpf_addr = chunk_offset(c->chunk);
                chunk_emit_opcode(c->chunk, FL_OP_JMPF);
                chunk_emit_addr(c->chunk, 0);  /* placeholder for exit address */

                /* Compile body */
                compile_node(c, node->data.for_stmt.body);

                /* Compile update expression (if present) */
                if (node->data.for_stmt.update) {
                    compile_node(c, node->data.for_stmt.update);
                }

                /* Emit JMP back to loop start */
                chunk_emit_opcode(c->chunk, FL_OP_JMP);
                chunk_emit_addr(c->chunk, (uint32_t)loop_start);

                /* Patch JMPF to jump past loop */
                uint32_t exit_offset = (uint32_t)chunk_offset(c->chunk);
                c->chunk->code[jmpf_addr + 1] = (exit_offset >> 24) & 0xFF;
                c->chunk->code[jmpf_addr + 2] = (exit_offset >> 16) & 0xFF;
                c->chunk->code[jmpf_addr + 3] = (exit_offset >> 8) & 0xFF;
                c->chunk->code[jmpf_addr + 4] = exit_offset & 0xFF;
            } else {
                /* Infinite loop (no test condition) */
                compile_node(c, node->data.for_stmt.body);

                /* Compile update expression (if present) */
                if (node->data.for_stmt.update) {
                    compile_node(c, node->data.for_stmt.update);
                }

                /* Emit JMP back to loop start */
                chunk_emit_opcode(c->chunk, FL_OP_JMP);
                chunk_emit_addr(c->chunk, (uint32_t)loop_start);
            }
            break;
        }

        case NODE_FN_EXPR: {
            /* Compile function expression: fn(param1, param2) { body }
             * Creates a closure with captured variables
             *
             * Phase 7-B: Variable Capture Analysis
             * - Analyze which outer-scope variables are referenced in the function body
             * - Create MAKE_CLOSURE opcode with captured variable list
             */
            int param_count = node->data.fn_expr.param_count;
            char **param_names = node->data.fn_expr.param_names;
            fl_ast_node_t *body = node->data.fn_expr.body;

            /* Generate unique name for anonymous function */
            static int anon_fn_counter = 0;
            char fn_name[256];
            snprintf(fn_name, 255, "__anon_fn_%d", anon_fn_counter++);

            fprintf(stderr, "[FN_EXPR] Compiling %s with %d parameters, scope_level=%d\n",
                    fn_name, param_count, c->scope_level);

            /* Create function value */
            fl_function_t *func = (fl_function_t *)malloc(sizeof(fl_function_t));
            if (!func) return;

            func->name = (char *)malloc(strlen(fn_name) + 1);
            strcpy(func->name, fn_name);
            func->arity = param_count;
            func->is_native = false;
            func->native_func = NULL;
            func->consts = NULL;
            func->const_count = 0;

            /* Compile function body to separate chunk */
            Chunk *fn_chunk = chunk_new();
            if (!fn_chunk) {
                free(func->name);
                free(func);
                return;
            }

            /* Save current compiler state */
            Chunk *saved_chunk = c->chunk;
            int saved_local_count = c->local_count;
            int saved_scope_level = c->scope_level;
            int saved_captured_count = c->captured_count;

            CapturedVarInfo saved_captured[256];
            memcpy(saved_captured, c->captured_vars, sizeof(saved_captured));

            /* Enter new scope for function body */
            c->chunk = fn_chunk;
            c->local_count = 0;
            c->scope_level++;
            clear_captured_vars(c);

            fprintf(stderr, "[FN_EXPR] Entering scope level %d\n", c->scope_level);

            /* Add parameters as locals in new scope */
            for (int i = 0; i < param_count; i++) {
                if (param_names && param_names[i]) {
                    compiler_add_local(c, param_names[i]);
                }
            }

            /* PHASE 7-B: Analyze body for variable captures
             * Find all identifiers that reference outer-scope variables
             */
            fprintf(stderr, "[CAPTURE_ANALYSIS] Analyzing body for captured variables\n");
            if (body) {
                analyze_captures(c, body, saved_scope_level);
            }

            /* Log captured variables */
            fprintf(stderr, "[CAPTURE_ANALYSIS] Found %d captured variables\n",
                    c->captured_count);
            for (int i = 0; i < c->captured_count; i++) {
                fprintf(stderr, "  - %s (scope_level=%d)\n",
                        c->captured_vars[i].name, c->captured_vars[i].scope_level);
            }

            /* Compile function body */
            if (body) {
                compile_node(c, body);
            }

            /* If no explicit return, push null */
            chunk_emit_opcode(fn_chunk, FL_OP_PUSH_NULL);
            chunk_emit_opcode(fn_chunk, FL_OP_RET);

            /* Store bytecode in function */
            func->bytecode = fn_chunk->code;
            func->bytecode_size = fn_chunk->code_len;

            /* Copy constants pool */
            if (fn_chunk->const_len > 0) {
                func->consts = (fl_value_t *)malloc(fn_chunk->const_len * sizeof(fl_value_t));
                if (func->consts) {
                    memcpy(func->consts, fn_chunk->consts,
                           fn_chunk->const_len * sizeof(fl_value_t));
                    func->const_count = fn_chunk->const_len;
                }
            }

            /* Restore compiler state */
            c->chunk = saved_chunk;
            c->local_count = saved_local_count;
            c->scope_level = saved_scope_level;

            int captured_count = c->captured_count;
            memcpy(c->captured_vars, saved_captured, sizeof(saved_captured));
            c->captured_count = saved_captured_count;

            fprintf(stderr, "[FN_EXPR] Exiting scope level, saved_scope_level=%d\n",
                    saved_scope_level);

            /* Register function in compiler's function table */
            if (c->function_count < 256) {
                c->functions[c->function_count++] = func;
            }

            int fn_idx = c->function_count - 1;

            /* PHASE 7-B: Emit MAKE_CLOSURE opcode
             * Stack layout before:
             *   (empty or arbitrary)
             *
             * Operands:
             *   - fn_idx (uint32): index of function in functions table
             *   - captured_count (uint8): number of captured variables
             *   - For each captured var: index of variable on stack
             *
             * Stack layout after:
             *   closure_value
             */
            fprintf(stderr, "[MAKE_CLOSURE] Creating closure with %d captured variables\n",
                    captured_count);

            /* Emit MAKE_CLOSURE instruction */
            chunk_emit_opcode(c->chunk, FL_OP_MAKE_CLOSURE);
            chunk_emit_addr(c->chunk, (uint32_t)fn_idx);

            /* Emit captured variable count */
            if (c->chunk->code_len >= c->chunk->code_capacity) {
                c->chunk->code_capacity *= 2;
                uint8_t* new_code = (uint8_t*)realloc(c->chunk->code, c->chunk->code_capacity);
                if (new_code) {
                    c->chunk->code = new_code;
                }
            }
            c->chunk->code[c->chunk->code_len++] = (uint8_t)captured_count;

            /* For each captured variable, emit its local index
             * This tells the VM where to find the value on the stack/locals
             */
            for (int i = 0; i < captured_count; i++) {
                int local_idx = saved_captured[i].local_idx;
                fprintf(stderr, "[MAKE_CLOSURE]   - Variable %d: local_idx=%d\n", i, local_idx);

                if (c->chunk->code_len >= c->chunk->code_capacity) {
                    c->chunk->code_capacity *= 2;
                    uint8_t* new_code = (uint8_t*)realloc(c->chunk->code, c->chunk->code_capacity);
                    if (new_code) {
                        c->chunk->code = new_code;
                    }
                }
                c->chunk->code[c->chunk->code_len++] = (uint8_t)local_idx;
            }

            break;
        }

        case NODE_ARROW_FN: {
            /* Compile arrow function: (params) => body or (params) => { body }
             * Creates a closure with captured variables
             *
             * Phase 7-B: Variable Capture Analysis
             * - Analyze which outer-scope variables are referenced
             * - Create MAKE_CLOSURE opcode with captured variable list
             */
            int param_count = node->data.arrow_fn.param_count;
            char **param_names = node->data.arrow_fn.param_names;
            fl_ast_node_t *body = node->data.arrow_fn.body;
            int is_expression = node->data.arrow_fn.is_expression;

            /* Generate unique name for anonymous function */
            static int anon_arrow_counter = 0;
            char fn_name[256];
            snprintf(fn_name, 255, "__arrow_fn_%d", anon_arrow_counter++);

            fprintf(stderr, "[ARROW_FN] Compiling %s with %d parameters, scope_level=%d\n",
                    fn_name, param_count, c->scope_level);

            /* Create function value */
            fl_function_t *func = (fl_function_t *)malloc(sizeof(fl_function_t));
            if (!func) return;

            func->name = (char *)malloc(strlen(fn_name) + 1);
            strcpy(func->name, fn_name);
            func->arity = param_count;
            func->is_native = false;
            func->native_func = NULL;
            func->consts = NULL;
            func->const_count = 0;

            /* Compile function body to separate chunk */
            Chunk *fn_chunk = chunk_new();
            if (!fn_chunk) {
                free(func->name);
                free(func);
                return;
            }

            /* Save current compiler state */
            Chunk *saved_chunk = c->chunk;
            int saved_local_count = c->local_count;
            int saved_scope_level = c->scope_level;
            int saved_captured_count = c->captured_count;

            CapturedVarInfo saved_captured[256];
            memcpy(saved_captured, c->captured_vars, sizeof(saved_captured));

            /* Enter new scope for function body */
            c->chunk = fn_chunk;
            c->local_count = 0;
            c->scope_level++;
            clear_captured_vars(c);

            fprintf(stderr, "[ARROW_FN] Entering scope level %d\n", c->scope_level);

            /* Add parameters as locals in new scope */
            for (int i = 0; i < param_count; i++) {
                if (param_names && param_names[i]) {
                    compiler_add_local(c, param_names[i]);
                }
            }

            /* PHASE 7-B: Analyze body for variable captures */
            fprintf(stderr, "[CAPTURE_ANALYSIS] Analyzing arrow function body for captured variables\n");
            if (body) {
                analyze_captures(c, body, saved_scope_level);
            }

            /* Log captured variables */
            fprintf(stderr, "[CAPTURE_ANALYSIS] Found %d captured variables\n",
                    c->captured_count);
            for (int i = 0; i < c->captured_count; i++) {
                fprintf(stderr, "  - %s (scope_level=%d)\n",
                        c->captured_vars[i].name, c->captured_vars[i].scope_level);
            }

            /* Compile function body */
            if (body) {
                if (is_expression) {
                    compile_node(c, body);
                } else {
                    compile_node(c, body);
                }
            }

            /* If no explicit return, push null */
            chunk_emit_opcode(fn_chunk, FL_OP_PUSH_NULL);
            chunk_emit_opcode(fn_chunk, FL_OP_RET);

            /* Store bytecode in function */
            func->bytecode = fn_chunk->code;
            func->bytecode_size = fn_chunk->code_len;

            /* Copy constants pool */
            if (fn_chunk->const_len > 0) {
                func->consts = (fl_value_t *)malloc(fn_chunk->const_len * sizeof(fl_value_t));
                if (func->consts) {
                    memcpy(func->consts, fn_chunk->consts,
                           fn_chunk->const_len * sizeof(fl_value_t));
                    func->const_count = fn_chunk->const_len;
                }
            }

            /* Restore compiler state */
            c->chunk = saved_chunk;
            c->local_count = saved_local_count;
            c->scope_level = saved_scope_level;

            int captured_count = c->captured_count;
            memcpy(c->captured_vars, saved_captured, sizeof(saved_captured));
            c->captured_count = saved_captured_count;

            fprintf(stderr, "[ARROW_FN] Exiting scope level, saved_scope_level=%d\n",
                    saved_scope_level);

            /* Register function in compiler's function table */
            if (c->function_count < 256) {
                c->functions[c->function_count++] = func;
            }

            int fn_idx = c->function_count - 1;

            /* PHASE 7-B: Emit MAKE_CLOSURE opcode with captured variables */
            fprintf(stderr, "[MAKE_CLOSURE] Creating arrow closure with %d captured variables\n",
                    captured_count);

            /* Emit MAKE_CLOSURE instruction */
            chunk_emit_opcode(c->chunk, FL_OP_MAKE_CLOSURE);
            chunk_emit_addr(c->chunk, (uint32_t)fn_idx);

            /* Emit captured variable count */
            if (c->chunk->code_len >= c->chunk->code_capacity) {
                c->chunk->code_capacity *= 2;
                uint8_t* new_code = (uint8_t*)realloc(c->chunk->code, c->chunk->code_capacity);
                if (new_code) {
                    c->chunk->code = new_code;
                }
            }
            c->chunk->code[c->chunk->code_len++] = (uint8_t)captured_count;

            /* For each captured variable, emit its local index */
            for (int i = 0; i < captured_count; i++) {
                int local_idx = saved_captured[i].local_idx;
                fprintf(stderr, "[MAKE_CLOSURE]   - Variable %d: local_idx=%d\n", i, local_idx);

                if (c->chunk->code_len >= c->chunk->code_capacity) {
                    c->chunk->code_capacity *= 2;
                    uint8_t* new_code = (uint8_t*)realloc(c->chunk->code, c->chunk->code_capacity);
                    if (new_code) {
                        c->chunk->code = new_code;
                    }
                }
                c->chunk->code[c->chunk->code_len++] = (uint8_t)local_idx;
            }

            break;
        }

        case NODE_FN_DECL: {
            /* Compile function declaration */
            /* fn name(param1, param2) { body } */
            const char *fn_name = node->data.fn_decl.name;
            int param_count = node->data.fn_decl.param_count;
            char **param_names = node->data.fn_decl.param_names;
            fl_ast_node_t *body = node->data.fn_decl.body;

            fprintf(stderr, "[COMPILER] Compiling function: %s with %d params\n",
                    fn_name, param_count);

            /* Create function value */
            fl_function_t *func = (fl_function_t *)malloc(sizeof(fl_function_t));
            if (!func) return;

            func->name = (char *)malloc(strlen(fn_name) + 1);
            strcpy(func->name, fn_name);
            func->arity = param_count;
            func->is_native = false;
            func->native_func = NULL;
            func->consts = NULL;
            func->const_count = 0;

            /* Compile function body to separate chunk */
            Chunk *fn_chunk = chunk_new();
            if (!fn_chunk) {
                free(func->name);
                free(func);
                return;
            }

            /* Save current compiler state */
            Chunk *saved_chunk = c->chunk;
            int saved_local_count = c->local_count;
            c->chunk = fn_chunk;
            c->local_count = 0;

            /* Add parameters as locals */
            for (int i = 0; i < param_count; i++) {
                if (param_names && param_names[i]) {
                    compiler_add_local(c, param_names[i]);  /* Track parameter names in compiler */
                }
            }

            /* Compile function body */
            if (body) {
                compile_node(c, body);
            }

            /* If no explicit return, push null */
            chunk_emit_opcode(fn_chunk, FL_OP_PUSH_NULL);
            chunk_emit_opcode(fn_chunk, FL_OP_RET);

            /* Store bytecode in function */
            func->bytecode = fn_chunk->code;
            func->bytecode_size = fn_chunk->code_len;

            /* Copy constants pool */
            if (fn_chunk->const_len > 0) {
                func->consts = (fl_value_t *)malloc(fn_chunk->const_len * sizeof(fl_value_t));
                if (func->consts) {
                    memcpy(func->consts, fn_chunk->consts,
                           fn_chunk->const_len * sizeof(fl_value_t));
                    func->const_count = fn_chunk->const_len;
                }
            }

            fprintf(stderr, "[COMPILER] Function %s compiled: %zu bytes, %d consts\n",
                    fn_name, fn_chunk->code_len, fn_chunk->const_len);
            /* Debug: dump bytecode */
            fprintf(stderr, "[COMPILER] Bytecode dump: ");
            for (size_t i = 0; i < fn_chunk->code_len && i < 32; i++) {
                fprintf(stderr, "%02x ", fn_chunk->code[i]);
            }
            fprintf(stderr, "\n");

            /* Also dump program bytecode */
            if (c->chunk) {
                fprintf(stderr, "[COMPILER] Program bytecode dump: ");
                for (size_t i = 0; i < c->chunk->code_len && i < 64; i++) {
                    fprintf(stderr, "%02x ", c->chunk->code[i]);
                }
                fprintf(stderr, "\n");
            }

            /* Restore compiler state */
            c->chunk = saved_chunk;
            c->local_count = saved_local_count;

            /* Register function in compiler's function table */
            if (c->function_count < 256) {
                c->functions[c->function_count++] = func;
            }

            break;
        }

        case NODE_ARRAY: {
            /* Compile array literal: [1, 2, 3, ...] */
            int elem_count = node->data.array.element_count;

            /* Emit each element */
            for (int i = 0; i < elem_count; i++) {
                if (node->data.array.elements && node->data.array.elements[i]) {
                    compile_node(c, node->data.array.elements[i]);
                }
            }

            /* Create array */
            chunk_emit_opcode(c->chunk, FL_OP_ARRAY_NEW);
            chunk_emit_addr(c->chunk, (uint32_t)elem_count);
            break;
        }

        case NODE_MEMBER: {
            /* Member access: obj.prop or obj[prop] */
            if (!node->data.member.object || !node->data.member.property) break;

            if (node->data.member.is_computed) {
                /* Array/Object index access: obj[prop] */
                compile_node(c, node->data.member.object);
                compile_node(c, node->data.member.property);
                chunk_emit_opcode(c->chunk, FL_OP_ARRAY_GET);
            } else {
                /* Property access: obj.prop */
                compile_node(c, node->data.member.object);

                /* Get property name */
                if (node->data.member.property->type == NODE_IDENT) {
                    const char* prop_name = node->data.member.property->data.ident.name;

                    /* Get object type and emit appropriate opcode */
                    if (strcmp(prop_name, "len") == 0 || strcmp(prop_name, "length") == 0) {
                        /* Generic length for both arrays and strings */
                        chunk_emit_opcode(c->chunk, FL_OP_ARRAY_LEN);  /* Works for strings too */
                    } else if (strcmp(prop_name, "push") == 0) {
                        int str_idx = chunk_emit_string(c->chunk, "push");
                        chunk_emit_opcode(c->chunk, FL_OP_PUSH_STRING);
                        chunk_emit_addr(c->chunk, (uint32_t)str_idx);
                    } else if (strcmp(prop_name, "pop") == 0) {
                        int str_idx = chunk_emit_string(c->chunk, "pop");
                        chunk_emit_opcode(c->chunk, FL_OP_PUSH_STRING);
                        chunk_emit_addr(c->chunk, (uint32_t)str_idx);
                    } else if (strcmp(prop_name, "upper") == 0 || strcmp(prop_name, "toUpperCase") == 0) {
                        int str_idx = chunk_emit_string(c->chunk, "upper");
                        chunk_emit_opcode(c->chunk, FL_OP_PUSH_STRING);
                        chunk_emit_addr(c->chunk, (uint32_t)str_idx);
                    } else if (strcmp(prop_name, "lower") == 0 || strcmp(prop_name, "toLowerCase") == 0) {
                        int str_idx = chunk_emit_string(c->chunk, "lower");
                        chunk_emit_opcode(c->chunk, FL_OP_PUSH_STRING);
                        chunk_emit_addr(c->chunk, (uint32_t)str_idx);
                    } else {
                        /* Object property access: push key string + OBJECT_GET */
                        int str_idx = chunk_emit_string(c->chunk, prop_name);
                        chunk_emit_opcode(c->chunk, FL_OP_PUSH_STRING);
                        chunk_emit_addr(c->chunk, (uint32_t)str_idx);
                        chunk_emit_opcode(c->chunk, FL_OP_OBJECT_GET);
                    }
                }
            }
            break;
        }

        /* Phase 6: NODE_VECTORIZE_HINT - @vectorize { body }
           컴파일러는 body를 그대로 컴파일하되 SIMD 힌트 NOP 마커 삽입
           (실제 SIMD 변환은 GCC -O3 -ftree-vectorize에 위임) */
        case NODE_VECTORIZE_HINT: {
            /* SIMD 마커: FL_OP_NOP로 표시 (향후 SIMD 코드 범위 표시) */
            chunk_emit_opcode(c->chunk, FL_OP_NOP);
            if (node->data.vectorize_hint.body) {
                compile_node(c, node->data.vectorize_hint.body);
            }
            chunk_emit_opcode(c->chunk, FL_OP_NOP);
            break;
        }

        /* Phase 6: NODE_ALIGNED_DECL - aligned let x = ...
           aligned는 컴파일러 힌트: 내부 decl을 그대로 컴파일 */
        case NODE_ALIGNED_DECL: {
            if (node->data.aligned_decl.decl) {
                compile_node(c, node->data.aligned_decl.decl);
            }
            break;
        }

        default:
            break;
    }
}
