/**
 * FreeLang Runtime Environment Implementation
 * Phase 3-C: Full Execution Pipeline
 *
 * Pipeline: Lexer → Parser → Compiler → VM
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../include/runtime.h"
#include "../include/vm.h"
#include "../include/gc.h"
#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/compiler.h"
#include "../include/ast.h"
#include "../include/token.h"

/* ============================================================
   Runtime Creation/Destruction
   ============================================================ */

fl_runtime_t* fl_runtime_create(void) {
    fl_runtime_t* runtime = (fl_runtime_t*)malloc(sizeof(fl_runtime_t));
    if (!runtime) return NULL;

    runtime->vm = fl_vm_create();
    if (!runtime->vm) {
        free(runtime);
        return NULL;
    }

    /* Set runtime reference in VM for user-defined functions */
    runtime->vm->runtime = (void*)runtime;

    runtime->gc = fl_gc_create();
    if (!runtime->gc) {
        fl_vm_destroy(runtime->vm);
        free(runtime);
        return NULL;
    }

    /* Create global store */
    runtime->globals = (GlobalStore*)malloc(sizeof(GlobalStore));
    if (!runtime->globals) {
        fl_gc_destroy(runtime->gc);
        fl_vm_destroy(runtime->vm);
        free(runtime);
        return NULL;
    }
    runtime->globals->capacity = 64;
    runtime->globals->count = 0;
    runtime->globals->keys = (char**)malloc(64 * sizeof(char*));
    runtime->globals->values = (fl_value_t*)malloc(64 * sizeof(fl_value_t));
    if (!runtime->globals->keys || !runtime->globals->values) {
        free(runtime->globals->keys);
        free(runtime->globals->values);
        free(runtime->globals);
        fl_gc_destroy(runtime->gc);
        fl_vm_destroy(runtime->vm);
        free(runtime);
        return NULL;
    }

    /* Initialize user-defined functions table */
    runtime->user_func_capacity = 64;
    runtime->user_func_count = 0;
    runtime->user_functions = (struct {
        char *name;
        fl_function_t *func;
    } *)malloc(runtime->user_func_capacity * sizeof(*runtime->user_functions));
    if (!runtime->user_functions) {
        free(runtime->globals->keys);
        free(runtime->globals->values);
        free(runtime->globals);
        fl_gc_destroy(runtime->gc);
        fl_vm_destroy(runtime->vm);
        free(runtime);
        return NULL;
    }

    runtime->builtins = NULL;
    runtime->modules = NULL;
    runtime->last_error = NULL;

    return runtime;
}

void fl_runtime_destroy(fl_runtime_t* runtime) {
    if (runtime) {
        /* Destroy VM */
        if (runtime->vm) {
            fl_vm_destroy(runtime->vm);
        }

        /* Destroy GC */
        if (runtime->gc) {
            fl_gc_destroy(runtime->gc);
        }

        /* Free globals */
        if (runtime->globals) {
            for (size_t i = 0; i < runtime->globals->count; i++) {
                if (runtime->globals->keys[i]) {
                    free(runtime->globals->keys[i]);
                }
                /* Values freed by GC */
            }
            free(runtime->globals->keys);
            free(runtime->globals->values);
            free(runtime->globals);
        }

        /* Free user-defined functions */
        if (runtime->user_functions) {
            for (int i = 0; i < runtime->user_func_count; i++) {
                if (runtime->user_functions[i].name) {
                    free(runtime->user_functions[i].name);
                }
                if (runtime->user_functions[i].func) {
                    if (runtime->user_functions[i].func->name) {
                        free(runtime->user_functions[i].func->name);
                    }
                    if (runtime->user_functions[i].func->bytecode) {
                        free(runtime->user_functions[i].func->bytecode);
                    }
                    if (runtime->user_functions[i].func->consts) {
                        free(runtime->user_functions[i].func->consts);
                    }
                    free(runtime->user_functions[i].func);
                }
            }
            free(runtime->user_functions);
        }

        /* Free error state */
        if (runtime->last_error) {
            if (runtime->last_error->message) {
                free(runtime->last_error->message);
            }
            free(runtime->last_error);
        }

        free(runtime);
    }
}

/* ============================================================
   Global Variable Management
   ============================================================ */

void fl_runtime_set_global(fl_runtime_t* runtime, const char* name, fl_value_t value) {
    if (!runtime || !runtime->globals || !name) {
        return;
    }

    GlobalStore* g = runtime->globals;

    /* Check if variable exists */
    for (size_t i = 0; i < g->count; i++) {
        if (g->keys[i] && strcmp(g->keys[i], name) == 0) {
            /* Update existing variable */
            g->values[i] = value;
            return;
        }
    }

    /* Resize if needed */
    if (g->count >= g->capacity) {
        g->capacity *= 2;
        char** new_keys = (char**)realloc(g->keys, g->capacity * sizeof(char*));
        fl_value_t* new_values = (fl_value_t*)realloc(g->values, g->capacity * sizeof(fl_value_t));
        if (!new_keys || !new_values) {
            return;
        }
        g->keys = new_keys;
        g->values = new_values;
    }

    /* Add new variable */
    g->keys[g->count] = (char*)malloc(strlen(name) + 1);
    if (!g->keys[g->count]) {
        return;
    }
    strcpy(g->keys[g->count], name);
    g->values[g->count] = value;
    g->count++;
}

fl_value_t fl_runtime_get_global(fl_runtime_t* runtime, const char* name) {
    fl_value_t result;
    result.type = FL_TYPE_NULL;

    if (!runtime || !runtime->globals || !name) {
        return result;
    }

    GlobalStore* g = runtime->globals;
    for (size_t i = 0; i < g->count; i++) {
        if (g->keys[i] && strcmp(g->keys[i], name) == 0) {
            return g->values[i];
        }
    }

    return result;
}

void fl_runtime_register_builtin(fl_runtime_t* runtime, const char* name,
                                 fl_value_t (*func)(fl_value_t*, size_t)) {
    /* TODO: Implement builtin registration */
    (void)runtime;
    (void)name;
    (void)func;
}

/* ============================================================
   User-defined Function Management
   ============================================================ */

void fl_runtime_register_function(fl_runtime_t* runtime, const char* name,
                                   fl_function_t* func) {
    if (!runtime || !name || !func) return;

    /* Resize if necessary */
    if (runtime->user_func_count >= runtime->user_func_capacity) {
        runtime->user_func_capacity *= 2;
        struct {
            char *name;
            fl_function_t *func;
        } *new_funcs = (struct {
            char *name;
            fl_function_t *func;
        } *)realloc(runtime->user_functions,
                    runtime->user_func_capacity * sizeof(*new_funcs));
        if (!new_funcs) return;
        runtime->user_functions = new_funcs;
    }

    /* Create a copy of the function (deep copy) */
    fl_function_t* func_copy = (fl_function_t*)malloc(sizeof(fl_function_t));
    if (!func_copy) return;

    /* Copy function data */
    func_copy->name = (char*)malloc(strlen(func->name) + 1);
    if (func_copy->name) {
        strcpy(func_copy->name, func->name);
    }
    /* Copy bytecode (deep copy - bytecode is NOT stable after compiler cleanup) */
    if (func->bytecode_size > 0) {
        func_copy->bytecode = (uint8_t*)malloc(func->bytecode_size);
        if (func_copy->bytecode) {
            memcpy(func_copy->bytecode, func->bytecode, func->bytecode_size);
        }
    } else {
        func_copy->bytecode = NULL;
    }
    func_copy->bytecode_size = func->bytecode_size;
    func_copy->arity = func->arity;
    func_copy->is_native = func->is_native;
    func_copy->native_func = func->native_func;

    /* Copy constants pool */
    if (func->const_count > 0) {
        func_copy->consts = (fl_value_t*)malloc(func->const_count * sizeof(fl_value_t));
        if (func_copy->consts) {
            memcpy(func_copy->consts, func->consts,
                   func->const_count * sizeof(fl_value_t));
        }
    } else {
        func_copy->consts = NULL;
    }
    func_copy->const_count = func->const_count;

    /* Store function copy */
    runtime->user_functions[runtime->user_func_count].name =
        (char *)malloc(strlen(name) + 1);
    if (runtime->user_functions[runtime->user_func_count].name) {
        strcpy(runtime->user_functions[runtime->user_func_count].name, name);
    }
    runtime->user_functions[runtime->user_func_count].func = func_copy;
    runtime->user_func_count++;

    fprintf(stderr, "[RUNTIME] Registered function copy: %s (bytecode=%p, consts=%d, is_native=%d)\n",
            name, (void*)func_copy->bytecode, func_copy->const_count, func_copy->is_native);
}

/* Find user-defined function by name */
fl_function_t* fl_runtime_find_function(fl_runtime_t* runtime, const char* name) {
    if (!runtime || !name) return NULL;

    fprintf(stderr, "[RUNTIME] Looking for function '%s' (total %d)\n", name, runtime->user_func_count);
    for (int i = 0; i < runtime->user_func_count; i++) {
        fprintf(stderr, "[RUNTIME]   [%d] '%s' (func=%p, is_native=%d)\n", i,
                runtime->user_functions[i].name ? runtime->user_functions[i].name : "(null)",
                (void*)runtime->user_functions[i].func,
                runtime->user_functions[i].func ? runtime->user_functions[i].func->is_native : -1);
        if (runtime->user_functions[i].name &&
            strcmp(runtime->user_functions[i].name, name) == 0) {
            fprintf(stderr, "[RUNTIME] FOUND! func=%p, is_native=%d\n",
                    (void*)runtime->user_functions[i].func,
                    runtime->user_functions[i].func->is_native);
            return runtime->user_functions[i].func;
        }
    }
    fprintf(stderr, "[RUNTIME] NOT FOUND\n");
    return NULL;
}

/* ============================================================
   Error Management
   ============================================================ */

fl_error_t* fl_runtime_get_error(fl_runtime_t* runtime) {
    if (!runtime) return NULL;
    return runtime->last_error;
}

void fl_runtime_clear_error(fl_runtime_t* runtime) {
    if (!runtime) return;

    if (runtime->last_error) {
        if (runtime->last_error->message) {
            free(runtime->last_error->message);
        }
        free(runtime->last_error);
        runtime->last_error = NULL;
    }
}

/* ============================================================
   Pipeline: Lexer → Parser → Compiler → VM
   ============================================================ */

fl_value_t fl_runtime_eval(fl_runtime_t* runtime, const char* source) {
    fl_value_t result;
    result.type = FL_TYPE_NULL;

    if (!runtime || !source) {
        return result;
    }

    /* Clear previous error */
    fl_runtime_clear_error(runtime);

    /* ========================================
       Step 1: Lexer - String → Tokens
       ======================================== */
    fprintf(stderr, "[RUNTIME] Step 1: Creating lexer\n");
    fflush(stderr);
    Lexer* lexer = lexer_new(source);
    fprintf(stderr, "[RUNTIME] lexer created: %p\n", (void*)lexer);
    fflush(stderr);
    if (!lexer) {
        runtime->last_error = (fl_error_t*)malloc(sizeof(fl_error_t));
        if (runtime->last_error) {
            runtime->last_error->type = FL_ERR_RUNTIME;
            runtime->last_error->message = (char*)malloc(64);
            if (runtime->last_error->message) {
                strcpy(runtime->last_error->message, "Lexer allocation failed");
            }
            runtime->last_error->line = 0;
            runtime->last_error->column = 0;
            runtime->last_error->filename = NULL;
            runtime->last_error->stack_trace = NULL;
        }
        return result;
    }

    if (lexer_scan_all(lexer) < 0) {
        runtime->last_error = (fl_error_t*)malloc(sizeof(fl_error_t));
        if (runtime->last_error) {
            runtime->last_error->type = FL_ERR_SYNTAX;
            runtime->last_error->message = (char*)malloc(64);
            if (runtime->last_error->message) {
                strcpy(runtime->last_error->message, "Tokenization failed");
            }
            runtime->last_error->line = 0;
            runtime->last_error->column = 0;
            runtime->last_error->filename = NULL;
            runtime->last_error->stack_trace = NULL;
        }
        lexer_free(lexer);
        return result;
    }

    /* ========================================
       Step 2: Parser - Tokens → AST
       ======================================== */
    fprintf(stderr, "[RUNTIME] Step 2: Creating parser (token_count=%d)\n", lexer->token_count);
    fl_parser_t* parser = fl_parser_create(
        lexer->tokens,  // Now a direct Token* array, not Token**
        lexer->token_count
    );
    fprintf(stderr, "[RUNTIME] parser created: %p\n", (void*)parser);
    if (!parser) {
        runtime->last_error = (fl_error_t*)malloc(sizeof(fl_error_t));
        if (runtime->last_error) {
            runtime->last_error->type = FL_ERR_RUNTIME;
            runtime->last_error->message = (char*)malloc(64);
            if (runtime->last_error->message) {
                strcpy(runtime->last_error->message, "Parser allocation failed");
            }
            runtime->last_error->line = 0;
            runtime->last_error->column = 0;
            runtime->last_error->filename = NULL;
            runtime->last_error->stack_trace = NULL;
        }
        lexer_free(lexer);
        return result;
    }

    fprintf(stderr, "[RUNTIME] Calling fl_parser_parse...\n");
    fflush(stderr);
    fl_ast_node_t* ast = fl_parser_parse(parser);
    fprintf(stderr, "[RUNTIME] fl_parser_parse returned: ast=%p\n", (void*)ast);
    fflush(stderr);
    if (!ast) {
        runtime->last_error = (fl_error_t*)malloc(sizeof(fl_error_t));
        if (runtime->last_error) {
            runtime->last_error->type = FL_ERR_SYNTAX;
            runtime->last_error->message = (char*)malloc(64);
            if (runtime->last_error->message) {
                strcpy(runtime->last_error->message, "Parse error: invalid AST");
            }
            runtime->last_error->line = 0;
            runtime->last_error->column = 0;
            runtime->last_error->filename = NULL;
            runtime->last_error->stack_trace = NULL;
        }
        fl_parser_destroy(parser);
        lexer_free(lexer);
        return result;
    }

    /* ========================================
       Step 3: Compiler - AST → Bytecode
       ======================================== */
    Compiler* compiler = compiler_new();
    if (!compiler) {
        runtime->last_error = (fl_error_t*)malloc(sizeof(fl_error_t));
        if (runtime->last_error) {
            runtime->last_error->type = FL_ERR_RUNTIME;
            runtime->last_error->message = (char*)malloc(64);
            if (runtime->last_error->message) {
                strcpy(runtime->last_error->message, "Compiler allocation failed");
            }
            runtime->last_error->line = 0;
            runtime->last_error->column = 0;
            runtime->last_error->filename = NULL;
            runtime->last_error->stack_trace = NULL;
        }
        fl_parser_destroy(parser);
        lexer_free(lexer);
        return result;
    }

    fprintf(stderr, "[RUNTIME] compiling AST to bytecode...\n");
    Chunk* chunk = compile_program(compiler, ast);
    fprintf(stderr, "[RUNTIME] compile result: chunk=%p code_len=%zu\n",
            (void*)chunk, chunk ? chunk->code_len : 0);
    if (!chunk || chunk->code_len == 0) {
        runtime->last_error = (fl_error_t*)malloc(sizeof(fl_error_t));
        if (runtime->last_error) {
            runtime->last_error->type = FL_ERR_RUNTIME;
            runtime->last_error->message = (char*)malloc(64);
            if (runtime->last_error->message) {
                strcpy(runtime->last_error->message, "Compilation failed");
            }
            runtime->last_error->line = 0;
            runtime->last_error->column = 0;
            runtime->last_error->filename = NULL;
            runtime->last_error->stack_trace = NULL;
        }
        compiler_free(compiler);
        fl_parser_destroy(parser);
        lexer_free(lexer);
        return result;
    }

    /* ========================================
       Step 3.5: Register user-defined functions
       ======================================== */
    fprintf(stderr, "[RUNTIME] registering user-defined functions (%d found)\n",
            compiler->function_count);
    for (int i = 0; i < compiler->function_count; i++) {
        if (compiler->functions[i]) {
            fl_runtime_register_function(runtime,
                                        compiler->functions[i]->name,
                                        compiler->functions[i]);
            fprintf(stderr, "[RUNTIME] registered function: %s\n",
                    compiler->functions[i]->name);
        }
    }

    /* ========================================
       Step 4: VM - Bytecode → Execution
       ======================================== */
    fl_vm_t* vm = runtime->vm;
    if (!vm) {
        runtime->last_error = (fl_error_t*)malloc(sizeof(fl_error_t));
        if (runtime->last_error) {
            runtime->last_error->type = FL_ERR_RUNTIME;
            runtime->last_error->message = (char*)malloc(64);
            if (runtime->last_error->message) {
                strcpy(runtime->last_error->message, "VM not initialized");
            }
            runtime->last_error->line = 0;
            runtime->last_error->column = 0;
            runtime->last_error->filename = NULL;
            runtime->last_error->stack_trace = NULL;
        }
        compiler_free(compiler);
        fl_parser_destroy(parser);
        lexer_free(lexer);
        return result;
    }

    /* Execute bytecode */
    result = fl_vm_execute(vm, chunk);

    /* ========================================
       Cleanup
       ======================================== */
    compiler_free(compiler);
    fl_parser_destroy(parser);
    lexer_free(lexer);
    ast_free_all();

    return result;
}

/* ============================================================
   File Execution
   ============================================================ */

fl_value_t fl_runtime_exec_file(fl_runtime_t* runtime, const char* filename) {
    fl_value_t result;
    result.type = FL_TYPE_NULL;

    if (!runtime || !filename) {
        return result;
    }

    /* Open file */
    FILE* file = fopen(filename, "r");
    if (!file) {
        runtime->last_error = (fl_error_t*)malloc(sizeof(fl_error_t));
        if (runtime->last_error) {
            runtime->last_error->type = FL_ERR_IO;
            runtime->last_error->message = (char*)malloc(256);
            if (runtime->last_error->message) {
                snprintf(runtime->last_error->message, 256, "Cannot open file: %s", filename);
            }
            runtime->last_error->line = 0;
            runtime->last_error->column = 0;
            runtime->last_error->filename = (char*)malloc(strlen(filename) + 1);
            if (runtime->last_error->filename) {
                strcpy(runtime->last_error->filename, filename);
            }
            runtime->last_error->stack_trace = NULL;
        }
        return result;
    }

    /* Get file size */
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size < 0) {
        fclose(file);
        runtime->last_error = (fl_error_t*)malloc(sizeof(fl_error_t));
        if (runtime->last_error) {
            runtime->last_error->type = FL_ERR_IO;
            runtime->last_error->message = (char*)malloc(64);
            if (runtime->last_error->message) {
                strcpy(runtime->last_error->message, "Failed to determine file size");
            }
            runtime->last_error->line = 0;
            runtime->last_error->column = 0;
            runtime->last_error->filename = NULL;
            runtime->last_error->stack_trace = NULL;
        }
        return result;
    }

    /* Allocate buffer */
    char* source = (char*)malloc((size_t)size + 1);
    if (!source) {
        fclose(file);
        runtime->last_error = (fl_error_t*)malloc(sizeof(fl_error_t));
        if (runtime->last_error) {
            runtime->last_error->type = FL_ERR_RUNTIME;
            runtime->last_error->message = (char*)malloc(64);
            if (runtime->last_error->message) {
                strcpy(runtime->last_error->message, "Memory allocation failed");
            }
            runtime->last_error->line = 0;
            runtime->last_error->column = 0;
            runtime->last_error->filename = NULL;
            runtime->last_error->stack_trace = NULL;
        }
        return result;
    }

    /* Read file */
    size_t bytes_read = fread(source, 1, (size_t)size, file);
    source[bytes_read] = '\0';
    fclose(file);

    /* Execute */
    result = fl_runtime_eval(runtime, source);

    /* Cleanup */
    free(source);

    return result;
}
