# FreeLang C Runtime - Complete Execution Flow

## End-to-End Pipeline Visualization

---

## 1. User Initiates Execution

### Option A: File Execution
```bash
$ ./bin/fl run program.fl
```

### Option B: Interactive REPL
```bash
$ ./bin/fl repl
fl> let x = 10;
fl> println(x);
fl> exit
```

### Option C: Tests
```bash
$ ./bin/fl test
```

---

## 2. Main Entry Point (main.c)

```
main(argc, argv)
│
├─ Parse command line
│
├─ "run" command
│  ├─ ast_init_pool(10000)
│  ├─ fl_runtime_create()
│  ├─ fl_runtime_exec_file(filename)
│  ├─ Check error via fl_runtime_get_error()
│  ├─ fl_runtime_destroy()
│  └─ ast_free_all()
│
├─ "repl" command
│  ├─ ast_init_pool(10000)
│  ├─ fl_runtime_create()
│  ├─ Loop:
│  │  ├─ printf("fl> ")
│  │  ├─ fgets(input)
│  │  ├─ Check for exit/quit
│  │  ├─ fl_runtime_eval(input)
│  │  ├─ fl_value_print(result)
│  │  └─ fl_runtime_clear_error()
│  ├─ fl_runtime_destroy()
│  └─ ast_free_all()
│
└─ "test" command
   ├─ ast_init_pool(10000)
   ├─ ast_pool_stats()
   └─ ast_free_all()
```

---

## 3. Runtime Execution (runtime.c)

### fl_runtime_exec_file(filename)
```
fl_runtime_exec_file(filename)
│
├─ fopen(filename, "r")
│
├─ fseek → SEEK_END
│
├─ ftell() → file size
│
├─ malloc(size + 1)
│
├─ fread(source)
│  └─ source[bytes_read] = '\0'
│
├─ fclose()
│
└─ fl_runtime_eval(source)  ← Continue to next section
   └─ return result
```

### fl_runtime_eval(source) - THE CORE PIPELINE

This is the heart of the runtime. It orchestrates all components:

```
fl_runtime_eval(source)
│
├─ fl_runtime_clear_error()
│
├─════════════════════════════════════════════════════════════
│  STEP 1: LEXER (Tokenization)
├─════════════════════════════════════════════════════════════
│
│  lexer_new(source)
│  ├─ Allocate Lexer struct
│  ├─ Initialize token buffer (256 capacity)
│  └─ return lexer
│
│  lexer_scan_all(lexer)
│  ├─ While current character != EOF:
│  │  ├─ Skip whitespace
│  │  ├─ Identify token type
│  │  ├─ Create Token struct
│  │  ├─ Add to token buffer
│  │  ├─ Resize if needed
│  │  └─ Advance position
│  ├─ Add TOK_EOF token
│  └─ return token_count
│
│  Result: lexer->tokens = [Token1, Token2, ..., TokenN]
│          lexer->token_count = N
│
├─════════════════════════════════════════════════════════════
│  STEP 2: PARSER (AST Generation)
├─════════════════════════════════════════════════════════════
│
│  fl_parser_create(tokens, token_count)
│  ├─ Allocate fl_parser_t struct
│  ├─ Set tokens pointer
│  ├─ Set token_count
│  ├─ Initialize pos = 0
│  └─ return parser
│
│  fl_parser_parse(parser)
│  ├─ While pos < token_count:
│  │  ├─ Get current token
│  │  ├─ Determine node type
│  │  ├─ Parse statement/expression
│  │  ├─ Create AST node
│  │  ├─ Add to program items
│  │  └─ Advance pos
│  ├─ Create NODE_PROGRAM with all items
│  └─ return root ast_node
│
│  Result: ast = fl_ast_node_t (NODE_PROGRAM)
│            ├─ items[0] = NODE_VAR_DECL
│            ├─ items[1] = NODE_BINARY
│            ├─ items[2] = NODE_CALL
│            └─ ...
│
├─════════════════════════════════════════════════════════════
│  STEP 3: COMPILER (Bytecode Generation)
├─════════════════════════════════════════════════════════════
│
│  compiler_new()
│  ├─ Allocate Compiler struct
│  ├─ chunk_new() → Create empty Chunk
│  │  ├─ code[256] = empty
│  │  ├─ consts[32] = empty
│  │  ├─ locals tracking
│  │  └─ line mapping
│  └─ return compiler
│
│  compile_program(compiler, ast)
│  ├─ For each item in ast->program.items:
│  │  ├─ Dispatch on node type
│  │  ├─ NODE_VAR_DECL:
│  │  │  ├─ compile_expr(initializer)
│  │  │  ├─ chunk_emit_string(name)
│  │  │  ├─ chunk_emit_opcode(STORE_GLOBAL)
│  │  │  └─ chunk_emit_addr(index)
│  │  ├─ NODE_BINARY:
│  │  │  ├─ compile_expr(left)
│  │  │  ├─ compile_expr(right)
│  │  │  └─ chunk_emit_opcode(OP_ADD/SUB/etc)
│  │  ├─ NODE_IF:
│  │  │  ├─ compile_expr(test)
│  │  │  ├─ chunk_emit_opcode(JMPF)
│  │  │  ├─ compile_stmt(consequent)
│  │  │  ├─ chunk_emit_opcode(JMP)
│  │  │  ├─ compile_stmt(alternate)
│  │  │  └─ Patch jump addresses
│  │  └─ ... other node types
│  ├─ chunk_emit_opcode(HALT)
│  └─ return chunk
│
│  Result: chunk->code = [OP, ARG1, ARG2, OP, ARG1, ..., HALT]
│          chunk->code_len = bytecode length
│
├─════════════════════════════════════════════════════════════
│  STEP 4: VIRTUAL MACHINE (Bytecode Execution)
├─════════════════════════════════════════════════════════════
│
│  fl_vm_execute(vm, bytecode, bytecode_len)
│  │
│  ├─ Initialize IP (instruction pointer) = 0
│  ├─ Initialize stack_top = 0
│  │
│  ├─ While IP < bytecode_len:
│  │  │
│  │  ├─ opcode = bytecode[IP++]
│  │  │
│  │  ├─ Switch(opcode)
│  │  │  ├─ PUSH_INT:
│  │  │  │  ├─ value = bytecode[IP++]
│  │  │  │  ├─ stack[stack_top++] = value
│  │  │  │  └─ Continue
│  │  │  │
│  │  │  ├─ ADD:
│  │  │  │  ├─ right = stack[--stack_top]
│  │  │  │  ├─ left = stack[--stack_top]
│  │  │  │  ├─ result = left + right
│  │  │  │  ├─ stack[stack_top++] = result
│  │  │  │  └─ Continue
│  │  │  │
│  │  │  ├─ STORE_GLOBAL:
│  │  │  │  ├─ name_idx = bytecode[IP++]
│  │  │  │  ├─ name = consts[name_idx].string
│  │  │  │  ├─ value = stack[--stack_top]
│  │  │  │  ├─ fl_vm_set_global(vm, name, value)
│  │  │  │  └─ Continue
│  │  │  │
│  │  │  ├─ LOAD_GLOBAL:
│  │  │  │  ├─ name_idx = bytecode[IP++]
│  │  │  │  ├─ name = consts[name_idx].string
│  │  │  │  ├─ value = fl_vm_get_global(vm, name)
│  │  │  │  ├─ stack[stack_top++] = value
│  │  │  │  └─ Continue
│  │  │  │
│  │  │  ├─ JMPF:
│  │  │  │  ├─ condition = stack[--stack_top]
│  │  │  │  ├─ addr = bytecode[IP..IP+3]
│  │  │  │  ├─ If !condition: IP = addr
│  │  │  │  └─ Continue
│  │  │  │
│  │  │  ├─ JMP:
│  │  │  │  ├─ addr = bytecode[IP..IP+3]
│  │  │  │  ├─ IP = addr
│  │  │  │  └─ Continue
│  │  │  │
│  │  │  ├─ CALL:
│  │  │  │  ├─ func = stack[--stack_top]
│  │  │  │  ├─ arg_count = bytecode[IP++]
│  │  │  │  ├─ Pop args from stack
│  │  │  │  ├─ Call function
│  │  │  │  ├─ Push result
│  │  │  │  └─ Continue
│  │  │  │
│  │  │  ├─ HALT:
│  │  │  │  └─ Break loop
│  │  │  │
│  │  │  └─ default:
│  │  │     └─ Unknown opcode error
│  │  │
│  │  └─ End Switch
│  │
│  └─ Return stack[stack_top-1] as result
│
│  Result: fl_value_t result
│           ├─ type = FL_TYPE_INT/STRING/etc
│           └─ data = computed value
│
├─════════════════════════════════════════════════════════════
│  STEP 5: CLEANUP
├─════════════════════════════════════════════════════════════
│
├─ compiler_free(compiler)
│  └─ chunk_free(chunk)
│     ├─ free(code)
│     ├─ free(consts)
│     └─ free(chunk)
│
├─ fl_parser_destroy(parser)
│
├─ lexer_free(lexer)
│  ├─ free(tokens)
│  └─ free(lexer)
│
├─ ast_free_all()
│  └─ Free all AST nodes from object pool
│
└─ return result
```

---

## 4. Error Handling Path

If any stage fails:

```
Step N encounters error
│
├─ malloc failure
│  └─ Set runtime->last_error
│
├─ Syntax error in lexer/parser
│  ├─ runtime->last_error->type = FL_ERR_SYNTAX
│  ├─ runtime->last_error->message = "Parse error..."
│  ├─ runtime->last_error->line = line_number
│  └─ runtime->last_error->column = col_number
│
├─ Runtime error in VM
│  ├─ runtime->last_error->type = FL_ERR_RUNTIME
│  └─ runtime->last_error->message = error description
│
└─ Return fl_value_t with type FL_TYPE_NULL
   (Caller checks runtime->last_error for details)
```

---

## 5. Global Variable Management

During execution, global variables are stored:

```
runtime->globals
├─ keys[0] = "x"
│  values[0] = fl_value_t { FL_TYPE_INT, 42 }
│
├─ keys[1] = "greeting"
│  values[1] = fl_value_t { FL_TYPE_STRING, "hello" }
│
├─ keys[2] = "flag"
│  values[2] = fl_value_t { FL_TYPE_BOOL, true }
│
├─ count = 3
├─ capacity = 64
└─ (Auto-grows 2x when count >= capacity)
```

**Operations**:
- `fl_runtime_set_global("x", val)` → Linear search, update or append
- `fl_runtime_get_global("x")` → Linear search, return value or NULL

---

## 6. Complete Data Flow Diagram

```
┌─────────────────────────────┐
│   Source Code String        │
│  "let x = 42; let y = x+1;" │
└────────────┬────────────────┘
             │
             ▼
     ┌──────────────┐
     │   LEXER.c    │
     │ lexer_new()  │
     │ scan_all()   │
     └──────┬───────┘
            │
            ▼
    ┌───────────────┐
    │  Token Array  │
    │  [LETT ID NUM │
    │   SEMI LET ID │
    │   EQ ID PLUS  │
    │   NUM SEMI]   │
    └───────┬───────┘
            │
            ▼
     ┌──────────────┐
     │  PARSER.c    │
     │fl_parser_    │
     │   parse()    │
     └──────┬───────┘
            │
            ▼
    ┌───────────────┐
    │    AST Tree   │
    │   PROGRAM     │
    │   ├─VAR_DECL  │
    │   │  name: x  │
    │   │  init: 42 │
    │   └─VAR_DECL  │
    │      name: y  │
    │      init:    │
    │        BINARY │
    │        x + 1  │
    └───────┬───────┘
            │
            ▼
   ┌────────────────┐
   │  COMPILER.c    │
   │ compile_       │
   │  program()     │
   └────────┬───────┘
            │
            ▼
   ┌────────────────┐
   │  Bytecode      │
   │  PUSH_INT 42   │
   │  STORE_GLB "x" │
   │  LOAD_GLB "x"  │
   │  PUSH_INT 1    │
   │  ADD           │
   │  STORE_GLB "y" │
   │  HALT          │
   └────────┬───────┘
            │
            ▼
     ┌──────────────┐
     │   VM.c       │
     │  fl_vm_      │
     │  execute()   │
     └──────┬───────┘
            │
            ▼
    ┌───────────────┐
    │  Globals      │
    │  x = 42       │
    │  y = 43       │
    │               │
    │  Stack (top)  │
    │  [43]         │
    └───────┬───────┘
            │
            ▼
    ┌───────────────┐
    │   Result      │
    │  fl_value_t   │
    │  type: INT    │
    │  data: 43     │
    └───────────────┘
```

---

## 7. REPL Loop Flow

```
REPL Loop
│
├─ printf("fl> ")
│
├─ fgets(input)
│
├─ strcmp(input, "exit") or strcmp(input, "quit")
│  ├─ Yes: break (exit REPL)
│  └─ No: continue
│
├─ fl_runtime_eval(input)
│  ├─ Full pipeline execution
│  └─ return result
│
├─ fl_runtime_get_error()
│  ├─ If error exists:
│  │  └─ printf("Error [%d]: %s\n", error->type, error->message)
│  ├─ fl_runtime_clear_error()
│  └─ continue
│
├─ If result.type != FL_TYPE_NULL:
│  ├─ printf("=> ")
│  ├─ fl_value_print(result)
│  └─ printf("\n")
│
└─ Back to step 1 (printf("fl> "))
```

---

## 8. Memory Allocation Timeline

```
Program Start
│
├─ ast_init_pool(10000)
│  └─ Allocate object pool for AST nodes
│
├─ fl_runtime_create()
│  ├─ malloc(fl_runtime_t)
│  ├─ fl_vm_create()
│  │  └─ malloc(fl_vm_t) with stack[10000]
│  ├─ fl_gc_create()
│  │  └─ malloc(fl_gc_t)
│  └─ malloc(GlobalStore) with capacity 64
│
├─ fl_runtime_eval("let x = 42;")
│  ├─ lexer_new() → malloc(Lexer)
│  ├─ lexer_scan_all() → malloc(tokens[256])
│  ├─ fl_parser_create() → malloc(fl_parser_t)
│  ├─ fl_parser_parse() → creates AST nodes (from pool)
│  ├─ compiler_new() → malloc(Compiler)
│  ├─ compile_program() → malloc(bytecode[256])
│  ├─ fl_vm_execute() → uses stack (no new malloc)
│  ├─ compiler_free() → free bytecode
│  ├─ fl_parser_destroy() → free parser
│  ├─ lexer_free() → free lexer + tokens
│  └─ ast_free_all() → return AST nodes to pool
│
├─ fl_runtime_destroy()
│  ├─ fl_vm_destroy()
│  ├─ fl_gc_destroy()
│  ├─ free(globals->keys)
│  ├─ free(globals->values)
│  ├─ free(globals)
│  └─ free(runtime)
│
└─ ast_free_pool()
   └─ Deallocate entire AST pool

Program End
```

---

## 9. Error Propagation

```
main.c
│
├─ fl_runtime_exec_file(filename)
│  │
│  ├─ fopen() fails
│  │  ├─ Set last_error->type = FL_ERR_IO
│  │  ├─ Set last_error->message = "Cannot open file: ..."
│  │  └─ return NULL value
│  │
│  └─ fl_runtime_eval() called
│     │
│     ├─ lexer_scan_all() fails
│     │  ├─ Set last_error->type = FL_ERR_SYNTAX
│     │  └─ return NULL value
│     │
│     ├─ fl_parser_parse() fails
│     │  ├─ Set last_error->type = FL_ERR_SYNTAX
│     │  └─ return NULL value
│     │
│     ├─ compile_program() fails
│     │  ├─ Set last_error->type = FL_ERR_RUNTIME
│     │  └─ return NULL value
│     │
│     └─ fl_vm_execute() fails
│        ├─ Set last_error->type = FL_ERR_RUNTIME
│        └─ return NULL value
│
└─ main.c checks error
   └─ fl_runtime_get_error() returns non-NULL
      ├─ Print error message with context
      └─ Exit with error code
```

---

## 10. Component Integration Points

```
                     main.c
                       │
           ┌───────────┼───────────┐
           │           │           │
      runtime.c    (calls)    (reports)
           │           │           │
    ┌──────┴────┐      ▼           │
 lexer.c   parser.c  compiler.c   vm.c
    │          │           │       │
    │          └───────┬───┘       │
    │                  │           │
    │     ┌────────────┴────┐      │
    │     │                 │      │
    └─────┤   ast.c (pool)  ├──────┘
          │                 │
          └────────────────token.c
                  (via)

Stack:
  main() calls runtime_eval()
    → lexer_new() + scan_all()
    → parser_create() + parse()
    → compiler_new() + compile()
    → vm_execute()
    → cleanup all

GC/Pool:
  AST nodes: managed by object pool (ast_free_all)
  Tokens: managed by lexer (lexer_free)
  Bytecode: managed by compiler (compiler_free)
  Values: managed by VM stack

Globals:
  Stored in runtime->globals
  Persist across eval() calls
  Freed in runtime_destroy()
```

---

## Summary

The complete execution pipeline is:

```
User Input
    ↓
main.c (parse args)
    ↓
runtime.c (fl_runtime_eval)
    ├─ lexer.c (tokenize)
    ├─ parser.c (parse AST)
    ├─ compiler.c (generate bytecode)
    ├─ vm.c (execute bytecode)
    └─ cleanup
    ↓
Result/Output
```

With error handling at each stage and comprehensive resource management throughout.
