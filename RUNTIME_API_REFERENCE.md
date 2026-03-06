# FreeLang C Runtime API Reference

## Complete API Documentation for Phase 3-C Runtime Pipeline

---

## Table of Contents

1. [Runtime Management](#runtime-management)
2. [Global Variables](#global-variables)
3. [Evaluation Functions](#evaluation-functions)
4. [Error Handling](#error-handling)
5. [Value Types](#value-types)
6. [Pipeline Architecture](#pipeline-architecture)
7. [Code Examples](#code-examples)

---

## Runtime Management

### fl_runtime_t* fl_runtime_create(void)

Creates a new runtime instance with initialized VM, GC, and global store.

**Parameters**: None

**Returns**:
- `fl_runtime_t*` - Pointer to new runtime, or NULL on allocation failure

**Allocates**: VM, GC, GlobalStore (256 initial variables)

**Example**:
```c
fl_runtime_t *runtime = fl_runtime_create();
if (!runtime) {
    fprintf(stderr, "Failed to create runtime\n");
    return 1;
}
```

---

### void fl_runtime_destroy(fl_runtime_t* runtime)

Frees all resources associated with the runtime.

**Parameters**:
- `runtime` - Runtime instance to destroy

**Returns**: Void

**Cleanup**:
- Destroys VM
- Destroys GC
- Frees all global variables
- Frees error state

**Example**:
```c
fl_runtime_destroy(runtime);
```

---

## Global Variables

### void fl_runtime_set_global(fl_runtime_t* runtime, const char* name, fl_value_t value)

Stores or updates a global variable.

**Parameters**:
- `runtime` - Runtime instance
- `name` - Variable name (C string)
- `value` - Value to store

**Returns**: Void

**Behavior**:
- If variable exists: updates value
- If variable not found: adds new entry
- Auto-resizes global store if capacity exceeded
- Max variable name: 255 chars

**Example**:
```c
fl_value_t val;
val.type = FL_TYPE_INT;
val.data.int_val = 42;
fl_runtime_set_global(runtime, "answer", val);

fl_value_t str_val;
str_val.type = FL_TYPE_STRING;
str_val.data.string_val = "hello";
fl_runtime_set_global(runtime, "greeting", str_val);
```

---

### fl_value_t fl_runtime_get_global(fl_runtime_t* runtime, const char* name)

Retrieves a global variable value.

**Parameters**:
- `runtime` - Runtime instance
- `name` - Variable name to lookup

**Returns**:
- `fl_value_t` - Variable value, or NULL-typed value if not found

**Behavior**:
- Returns fl_value_t with type FL_TYPE_NULL if variable not found
- Linear search O(n) where n = number of variables

**Example**:
```c
fl_value_t val = fl_runtime_get_global(runtime, "answer");
if (val.type == FL_TYPE_INT) {
    printf("Value: %ld\n", val.data.int_val);
} else if (val.type == FL_TYPE_NULL) {
    printf("Variable not found\n");
}
```

---

## Evaluation Functions

### fl_value_t fl_runtime_eval(fl_runtime_t* runtime, const char* source)

Evaluates a single line or statement of FreeLang source code.

**Parameters**:
- `runtime` - Runtime instance
- `source` - FreeLang source code string

**Returns**:
- `fl_value_t` - Result of evaluation

**Pipeline Steps** (internal):
1. **Lexer**: Tokenize source → tokens
2. **Parser**: Parse tokens → AST
3. **Compiler**: Compile AST → bytecode
4. **VM**: Execute bytecode → result

**Error Handling**:
- Sets `runtime->last_error` on failure
- Each stage has error checks
- Returns NULL value on error

**Example**:
```c
// Single statement
fl_value_t result = fl_runtime_eval(runtime, "let x = 10;");
if (runtime->last_error) {
    printf("Error: %s\n", runtime->last_error->message);
}

// Expression evaluation
fl_value_t sum = fl_runtime_eval(runtime, "5 + 3;");
```

---

### fl_value_t fl_runtime_exec_file(fl_runtime_t* runtime, const char* filename)

Reads and executes a FreeLang source file.

**Parameters**:
- `runtime` - Runtime instance
- `filename` - Path to .fl file

**Returns**:
- `fl_value_t` - Result of last statement in file

**Operations**:
1. Opens file in read mode
2. Determines file size
3. Allocates buffer
4. Reads entire file
5. Calls `fl_runtime_eval()`
6. Returns result

**Error Handling**:
- File not found → FL_ERR_IO
- Malloc failure → FL_ERR_RUNTIME
- Parse/compile errors → forwarded from eval()

**Example**:
```c
fl_value_t result = fl_runtime_exec_file(runtime, "program.fl");
fl_error_t *error = fl_runtime_get_error(runtime);
if (error) {
    printf("Error [%d]: %s\n", error->type, error->message);
    if (error->filename) printf("File: %s:%d\n", error->filename, error->line);
}
```

---

## Error Handling

### fl_error_t* fl_runtime_get_error(fl_runtime_t* runtime)

Retrieves the last error that occurred.

**Parameters**:
- `runtime` - Runtime instance

**Returns**:
- `fl_error_t*` - Pointer to error info, or NULL if no error

**Error Structure**:
```c
typedef struct fl_error {
    fl_error_type_t type;     // Error category
    char* message;            // Error description
    char* filename;           // Source filename (nullable)
    int line;                 // Line number (0 if unknown)
    int column;               // Column number (0 if unknown)
    char* stack_trace;        // Stack trace (nullable)
} fl_error_t;
```

**Error Types**:
```c
FL_ERR_NONE = 0               // No error
FL_ERR_SYNTAX = 1             // Parse error
FL_ERR_RUNTIME = 2            // Runtime error
FL_ERR_TYPE = 3               // Type mismatch
FL_ERR_REFERENCE = 4          // Unknown reference
FL_ERR_INDEX_OUT_OF_BOUNDS = 5
FL_ERR_DIVIDE_BY_ZERO = 6
FL_ERR_UNDEFINED_VARIABLE = 7
FL_ERR_UNDEFINED_FUNCTION = 8
FL_ERR_INVALID_ARGUMENT = 9
FL_ERR_ASSERTION_FAILED = 10
FL_ERR_IO = 11                // File I/O error
FL_ERR_CUSTOM = 12            // Custom error
```

**Example**:
```c
fl_error_t *error = fl_runtime_get_error(runtime);
if (error && error->type != FL_ERR_NONE) {
    fprintf(stderr, "ERROR[%d]: %s\n", error->type, error->message);
    if (error->filename && error->line > 0) {
        fprintf(stderr, "  at %s:%d:%d\n", error->filename, error->line, error->column);
    }
}
```

---

### void fl_runtime_clear_error(fl_runtime_t* runtime)

Clears the last error state.

**Parameters**:
- `runtime` - Runtime instance

**Returns**: Void

**Behavior**:
- Frees error memory
- Sets last_error to NULL
- Safe to call even if no error exists

**Example**:
```c
fl_runtime_clear_error(runtime);
// error state now cleared
```

---

## Value Types

### fl_value_t Structure

Represents any FreeLang value with runtime type information.

```c
typedef enum {
    FL_TYPE_NULL,      // Null/undefined
    FL_TYPE_BOOL,      // Boolean
    FL_TYPE_INT,       // Integer (int64_t)
    FL_TYPE_FLOAT,     // Float (double)
    FL_TYPE_STRING,    // String (char*)
    FL_TYPE_ARRAY,     // Array
    FL_TYPE_OBJECT,    // Object/map
    FL_TYPE_FUNCTION,  // Function
    FL_TYPE_ERROR      // Error value
} fl_type_t;

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
        struct fl_error* error_val;
    } data;
} fl_value_t;
```

### Value Construction Examples

**Integer**:
```c
fl_value_t val;
val.type = FL_TYPE_INT;
val.data.int_val = 42;
```

**String**:
```c
fl_value_t val;
val.type = FL_TYPE_STRING;
val.data.string_val = (char*)malloc(6);
strcpy(val.data.string_val, "hello");
```

**Boolean**:
```c
fl_value_t val;
val.type = FL_TYPE_BOOL;
val.data.bool_val = true;
```

**Null**:
```c
fl_value_t val;
val.type = FL_TYPE_NULL;
```

---

## Pipeline Architecture

### Lexer (tokenization)

**Input**: Source code string
**Output**: Token array

**Header**: `include/lexer.h`

```c
Lexer* lexer = lexer_new(source);
lexer_scan_all(lexer);  // Tokenize
Token *tokens = lexer->tokens;
size_t count = lexer->token_count;
lexer_free(lexer);
```

### Parser (AST generation)

**Input**: Token array
**Output**: Abstract Syntax Tree (fl_ast_node_t*)

**Header**: `include/parser.h`

```c
fl_parser_t* parser = fl_parser_create(tokens, token_count);
fl_ast_node_t* ast = fl_parser_parse(parser);
fl_parser_destroy(parser);
```

### Compiler (bytecode generation)

**Input**: AST
**Output**: Bytecode (Chunk*)

**Header**: `include/compiler.h`

```c
Compiler* compiler = compiler_new();
Chunk* chunk = compile_program(compiler, ast);
// chunk->code is bytecode array
// chunk->code_len is bytecode length
compiler_free(compiler);
```

### Virtual Machine (execution)

**Input**: Bytecode
**Output**: fl_value_t result

**Header**: `include/vm.h`

```c
fl_vm_t* vm = fl_vm_create();
fl_value_t result = fl_vm_execute(vm, bytecode, bytecode_len);
fl_vm_destroy(vm);
```

---

## Code Examples

### Example 1: Simple Evaluation

```c
#include <stdio.h>
#include "include/runtime.h"

int main(void) {
    // Create runtime
    fl_runtime_t *runtime = fl_runtime_create();
    if (!runtime) {
        fprintf(stderr, "Runtime creation failed\n");
        return 1;
    }

    // Evaluate code
    fl_value_t result = fl_runtime_eval(runtime, "let x = 42;");

    // Check errors
    fl_error_t *error = fl_runtime_get_error(runtime);
    if (error) {
        fprintf(stderr, "Error: %s\n", error->message);
    }

    // Cleanup
    fl_runtime_destroy(runtime);
    return 0;
}
```

### Example 2: File Execution

```c
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.fl>\n", argv[0]);
        return 1;
    }

    fl_runtime_t *runtime = fl_runtime_create();
    fl_value_t result = fl_runtime_exec_file(runtime, argv[1]);

    fl_error_t *error = fl_runtime_get_error(runtime);
    if (error) {
        fprintf(stderr, "ERROR[%d]: %s\n", error->type, error->message);
        if (error->filename) {
            fprintf(stderr, "  at %s:%d\n", error->filename, error->line);
        }
    }

    fl_runtime_destroy(runtime);
    return error ? 1 : 0;
}
```

### Example 3: Global Variables

```c
void demo_globals(void) {
    fl_runtime_t *runtime = fl_runtime_create();

    // Set globals
    fl_value_t val;
    val.type = FL_TYPE_INT;
    val.data.int_val = 100;
    fl_runtime_set_global(runtime, "x", val);

    val.data.int_val = 200;
    fl_runtime_set_global(runtime, "y", val);

    // Get globals
    fl_value_t x = fl_runtime_get_global(runtime, "x");
    fl_value_t y = fl_runtime_get_global(runtime, "y");

    if (x.type == FL_TYPE_INT && y.type == FL_TYPE_INT) {
        printf("x + y = %ld\n", x.data.int_val + y.data.int_val);
    }

    fl_runtime_destroy(runtime);
}
```

### Example 4: REPL Loop

```c
void interactive_repl(void) {
    fl_runtime_t *runtime = fl_runtime_create();
    char input[1024];

    printf("FreeLang REPL (type 'exit' to quit)\n");

    while (1) {
        printf("fl> ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;
        if (strcmp(input, "exit\n") == 0) break;

        fl_value_t result = fl_runtime_eval(runtime, input);

        fl_error_t *error = fl_runtime_get_error(runtime);
        if (error) {
            printf("Error: %s\n", error->message);
            fl_runtime_clear_error(runtime);
        } else if (result.type != FL_TYPE_NULL) {
            printf("=> ");
            fl_value_print(result);
            printf("\n");
        }
    }

    fl_runtime_destroy(runtime);
}
```

---

## Memory Management

### Allocation Strategy

- **Runtime**: Single allocation, freed with `fl_runtime_destroy()`
- **Globals**: Growable array, freed in `fl_runtime_destroy()`
- **Temp values**: Freed after each eval() call
- **Strings**: Allocated by compiler, freed by GC

### Best Practices

1. Always call `fl_runtime_create()` before use
2. Always call `fl_runtime_destroy()` before exit
3. Check `fl_runtime_get_error()` after eval/exec
4. Use `fl_runtime_clear_error()` to reset error state
5. Don't modify returned error pointers

---

## Thread Safety

**Current Status**: NOT thread-safe

**Limitations**:
- Single runtime per thread
- No mutex protection
- VM state not protected
- GC not synchronized

**Recommendation**:
- Create separate runtime per thread
- Use thread-local storage for runtime pointers

---

## Performance Characteristics

| Operation | Time Complexity | Space Complexity |
|-----------|-----------------|------------------|
| eval() | O(n) | O(n) |
| exec_file() | O(m) | O(m) |
| get_global() | O(v) | O(1) |
| set_global() | O(v) | O(1) |

Where:
- n = source length
- m = file size
- v = number of variables

---

## See Also

- `PHASE3C_RUNTIME_COMPLETE.md` - Implementation details
- `RUNTIME_QUICK_START.md` - Quick start guide
- `include/freelang.h` - Type definitions
- `src/runtime.c` - Implementation source

---

## Version

- **API Version**: 1.0
- **Phase**: 3-C (Runtime Pipeline)
- **Date**: 2026-03-06
- **Status**: ✅ Complete
