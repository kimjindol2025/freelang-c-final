# FreeLang C Runtime — 모듈별 인터페이스 명세

**문서**: 각 모듈의 핵심 데이터 구조 및 함수 인터페이스
**기준일**: 2026-03-06

---

## 1️⃣ Token 모듈 (`include/token.h`, `src/token.c`)

**라인**: 115줄 헤더 + 129줄 소스 = 244줄
**함수**: 4개

### 핵심 구조
```c
typedef struct {
    TokenType type;         // 토큰 타입 (62개)
    char *value;            // 렉심 텍스트 (메모리 소유)
    double num_value;       // 숫자 값
    int line, col;          // 위치
} Token;
```

### 함수
```c
Token *token_new(TokenType type, const char *value, int line, int col);
void   token_free(Token *tok);
const char *token_type_name(TokenType type);  // 디버깅
void   token_print(Token *tok);               // 프린트
```

### 토큰 타입 (62개)
- **Literals (4)**: NUMBER, STRING, FSTRING, IDENT
- **Keywords (28)**: LET, CONST, VAR, FN, IF, ELSE, WHILE, FOR, IN, RETURN, BREAK, CONTINUE, TRUE, FALSE, NULL, STRUCT, ENUM, TRY, CATCH, FINALLY, THROW, IMPORT, FROM, EXPORT, DEFAULT, AS, ASYNC, AWAIT, MATCH
- **Operators (26)**: PLUS, MINUS, STAR, SLASH, PERCENT, POWER, EQ, EQEQ, EQEQEQ, NOT, NOTEQ, NOTEQUALEQ, LT, LTEQ, GT, GTEQ, AND, OR, AMPERSAND, PIPE, CARET, TILDE, LSHIFT, RSHIFT, ARROW, QUESTION
- **Punctuation (13)**: LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET, COMMA, DOT, SEMICOLON, COLON, AT, HASH, DOLLAR
- **Compound (4)**: PLUS_EQ, MINUS_EQ, STAR_EQ, SLASH_EQ
- **Special (2)**: NEWLINE, EOF

---

## 2️⃣ Lexer 모듈 (`include/lexer.h`, `src/lexer.c`)

**라인**: 38줄 헤더 + 499줄 소스 = 537줄
**함수**: 18개

### 핵심 구조
```c
typedef struct {
    const char *src;            // 소스 코드 (읽기 전용)
    size_t src_len;
    size_t pos;                 // 현재 위치
    int line, col;              // 라인/컬럼 추적
    
    Token *tokens;              // 동적 배열
    size_t token_count;
    size_t token_capacity;
    
    void *keyword_map;          // HashMap<string, TokenType>
} Lexer;
```

### 함수
```c
Lexer  *lexer_new(const char *source);
void    lexer_free(Lexer *l);

int     lexer_scan_all(Lexer *l);           // 전체 토큰화
Token  *lexer_next_token(Lexer *l);         // 단일 토큰
Token  *lexer_peek_token(Lexer *l, int offset);  // 미리보기

Token  *lexer_get_token(Lexer *l, size_t index);
size_t  lexer_token_count(Lexer *l);
void    lexer_print_tokens(Lexer *l);       // 디버깅
```

### 기능
- 상태 머신 기반 렉싱
- 키워드 맵 (O(1) 조회)
- 동적 토큰 배열 (자동 리사이징)
- 라인/컬럼 추적

---

## 3️⃣ Parser 모듈 (`include/parser.h`, `src/parser.c`)

**라인**: 34줄 헤더 + 973줄 소스 = 1,007줄
**함수**: 40개
**방식**: Recursive descent

### 핵심 구조
```c
typedef struct fl_parser {
    fl_token_t* tokens;
    size_t token_count;
    size_t pos;
} fl_parser_t;
```

### 함수
```c
fl_parser_t* fl_parser_create(fl_token_t* tokens, size_t token_count);
void fl_parser_destroy(fl_parser_t* parser);
fl_ast_node_t* fl_parser_parse(fl_parser_t* parser);
fl_ast_node_t* fl_parser_parse_statement(fl_parser_t* parser);
fl_ast_node_t* fl_parser_parse_expression(fl_parser_t* parser);
fl_ast_node_t* fl_parser_parse_function(fl_parser_t* parser);
```

### 파싱 규칙
```
program       → (statement | declaration)*
declaration   → var_decl | fn_decl | struct_decl | enum_decl
statement     → expr_stmt | block | if_stmt | while_stmt | for_stmt | return | break | continue | try_stmt | throw
expression    → assignment
assignment    → ternary (("=" | "+=" | "-=" | "*=" | "/=") ternary)*
ternary       → logical_or ("?" expression ":" ternary)?
logical_or    → logical_and ("||" logical_and)*
logical_and   → bitwise_or ("&&" bitwise_or)*
... (15+ 더 많은 우선순위 레벨)
primary       → IDENT | NUMBER | STRING | "(" expression ")" | "fn" ... | "[" ... | "{"...
```

---

## 4️⃣ AST 모듈 (`include/ast.h`, `src/ast.c`)

**라인**: 586줄 헤더 + 606줄 소스 = 1,192줄
**함수**: 29개
**노드 타입**: 47개

### 핵심 구조
```c
typedef struct fl_ast_node {
    NodeType type;
    int line, col;
    TypeAnnotation *type_info;
    
    union {
        struct { ... } program;       // 프로그램
        struct { ... } var_decl;      // 변수 선언
        struct { ... } fn_decl;       // 함수 선언
        struct { ... } binary;        // 이항 연산
        struct { ... } call;          // 함수 호출
        // ... 43개 더
    } data;
} fl_ast_node_t;
```

### 노드 타입 (47개)
```
Program & Module (1):     NODE_PROGRAM
Declarations (4):         NODE_VAR_DECL, NODE_FN_DECL, NODE_STRUCT_DECL, NODE_ENUM_DECL
Module (2):               NODE_IMPORT, NODE_EXPORT
Statements (10):          NODE_BLOCK, NODE_IF, NODE_WHILE, NODE_FOR, NODE_FOR_IN, 
                          NODE_RETURN, NODE_BREAK, NODE_CONTINUE, NODE_TRY, NODE_THROW
Expressions (7):          NODE_BINARY, NODE_UNARY, NODE_LOGICAL, NODE_CALL, NODE_MEMBER, 
                          NODE_ASSIGN, NODE_TERNARY
Collections (3):          NODE_ARRAY, NODE_OBJECT, NODE_PROPERTY
Advanced (5):             NODE_FN_EXPR, NODE_ARROW_FN, NODE_AWAIT, NODE_FSTRING, NODE_QUESTION_OP
Identifiers (2):          NODE_IDENT, NODE_LITERAL
Clauses (2):              NODE_CATCH_CLAUSE, NODE_FINALLY_CLAUSE
```

### 함수
```c
void ast_init_pool(size_t capacity);
fl_ast_node_t *ast_alloc(NodeType type);
void ast_free_all(void);
void ast_free(fl_ast_node_t *node);
fl_ast_node_t *ast_clone(fl_ast_node_t *node);
void ast_print(fl_ast_node_t *node, int indent);
const char *ast_type_name(NodeType type);

// 헬퍼 함수들
fl_ast_node_t *ast_new_var_decl(const char *kind, const char *name, fl_ast_node_t *init);
fl_ast_node_t *ast_new_fn_decl(const char *name, char **params, int param_count, ...);
fl_ast_node_t *ast_new_binary(fl_ast_node_t *left, const char *op, fl_ast_node_t *right);
// ... 15+ 더 많은 헬퍼
```

### 객체 풀 (Object Pool)
```c
typedef struct {
    fl_ast_node_t *nodes;
    int *is_used;           // 0 = free, 1 = in use
    size_t capacity;
    size_t count;
} ASTNodePool;
```

---

## 5️⃣ Type Checker 모듈 (`include/typechecker.h`, `src/typechecker.c`)

**라인**: 575줄 헤더 + 945줄 소스 = 1,520줄
**함수**: 49개

### 핵심 구조
```c
typedef enum {
    FL_TYPE_NULL, FL_TYPE_BOOL, FL_TYPE_INT, FL_TYPE_FLOAT, FL_TYPE_STRING,
    FL_TYPE_ARRAY, FL_TYPE_OBJECT, FL_TYPE_FUNCTION, FL_TYPE_CLOSURE, FL_TYPE_ERROR
} fl_type_t;

typedef struct fl_value {
    fl_type_t type;
    union {
        fl_bool bool_val;
        fl_int int_val;             // int64_t
        fl_float float_val;         // double
        fl_string string_val;
        struct fl_array* array_val;
        struct fl_object* object_val;
        struct fl_function* func_val;
        struct fl_closure* closure_val;
        struct fl_error* error_val;
    } data;
} fl_value_t;
```

### 함수
```c
const char* fl_type_name(fl_type_t type);
void fl_value_print(fl_value_t value);
void fl_value_free(fl_value_t value);

// Type inference & checking
fl_type_t infer_type(fl_ast_node_t *node);
int is_type_compatible(fl_type_t from, fl_type_t to);
int is_numeric_type(fl_type_t type);
// ... 40+ 더 많은 함수
```

### Error Types (13개)
```c
FL_ERR_NONE, FL_ERR_SYNTAX, FL_ERR_RUNTIME, FL_ERR_TYPE, FL_ERR_REFERENCE,
FL_ERR_INDEX_OUT_OF_BOUNDS, FL_ERR_DIVIDE_BY_ZERO, FL_ERR_UNDEFINED_VARIABLE,
FL_ERR_UNDEFINED_FUNCTION, FL_ERR_INVALID_ARGUMENT, FL_ERR_ASSERTION_FAILED,
FL_ERR_IO, FL_ERR_CUSTOM
```

---

## 6️⃣ Compiler 모듈 (`include/compiler.h`, `src/compiler.c`)

**라인**: 158줄 헤더 + 1,312줄 소스 = 1,470줄
**함수**: 26개
**목표**: AST → Bytecode

### 핵심 구조
```c
typedef struct {
    uint8_t *code;              // Bytecode array
    size_t code_len;
    size_t code_capacity;
    
    fl_value_t *consts;         // Constants pool
    size_t const_len;
    size_t const_capacity;
    
    int locals_count;
    int *line_map;              // Debugging
    size_t line_map_capacity;
} Chunk;

typedef struct {
    Chunk *chunk;
    
    // Loop control
    int break_addrs[100];
    int break_count;
    int continue_addrs[100];
    int continue_count;
    
    // Local variables
    char local_names[256][256];
    int local_count;
    
    // Closure capture
    int scope_level;
    CapturedVarInfo captured_vars[256];
    int captured_count;
    
    // Functions table
    fl_function_t *functions[256];
    int function_count;
} Compiler;
```

### 함수
```c
Compiler* compiler_new(void);
void compiler_free(Compiler *c);

Chunk* compile_program(Compiler *c, fl_ast_node_t *node);
Chunk* compile_expr(Compiler *c, fl_ast_node_t *node);
Chunk* compile_stmt(Compiler *c, fl_ast_node_t *node);

Chunk* chunk_new(void);
void chunk_free(Chunk *chunk);
void chunk_emit_opcode(Chunk *chunk, fl_opcode_t opcode);
int chunk_emit_int(Chunk *chunk, fl_int value);
int chunk_emit_float(Chunk *chunk, fl_float value);
int chunk_emit_string(Chunk *chunk, const char *value);
int chunk_add_local(Chunk *chunk, const char *name);
int chunk_get_local(Chunk *chunk, const char *name);
void chunk_patch_jump(Chunk *chunk, int addr);
size_t chunk_offset(Chunk *chunk);
void chunk_emit_addr(Chunk *chunk, uint32_t addr);
// ... 10+ 더 많은 함수
```

---

## 7️⃣ VM 모듈 (`include/vm.h`, `src/vm.c`)

**라인**: 68줄 헤더 + 1,245줄 소스 = 1,313줄
**함수**: 19개
**아키텍처**: Stack-based bytecode VM

### 핵심 구조
```c
#define FL_VM_STACK_SIZE 10000
#define FL_VM_FRAME_SIZE 256

typedef struct fl_call_frame {
    uint8_t* ip;                    // Instruction pointer
    fl_value_t* locals;
    size_t local_count;
    struct fl_closure* closure;
} fl_call_frame_t;

typedef struct fl_vm {
    fl_value_t stack[FL_VM_STACK_SIZE];
    size_t stack_top;
    
    fl_call_frame_t frames[FL_VM_FRAME_SIZE];
    size_t frame_count;
    
    fl_value_t locals[256];
    size_t local_count;
    
    fl_value_t globals;
    void* gc;
    void* runtime;
    
    // Exception handling
    fl_value_t exception;
    bool exception_active;
    size_t exception_handler;
    
    // Closure context
    struct fl_closure* current_closure;
} fl_vm_t;
```

### 함수
```c
fl_vm_t* fl_vm_create(void);
void fl_vm_destroy(fl_vm_t* vm);
fl_value_t fl_vm_execute(fl_vm_t* vm, const void* chunk);

// Stack operations
void fl_vm_push(fl_vm_t* vm, fl_value_t value);
fl_value_t fl_vm_pop(fl_vm_t* vm);
fl_value_t fl_vm_peek(fl_vm_t* vm);

// Variable management
void fl_vm_set_global(fl_vm_t* vm, const char* name, fl_value_t value);
fl_value_t fl_vm_get_global(fl_vm_t* vm, const char* name);

// Function calls
fl_value_t fl_vm_call(fl_vm_t* vm, fl_value_t func, fl_value_t* args, size_t arg_count);
// ... 10+ 더 많은 함수
```

### Opcode 실행 (45개)
```c
// Stack: PUSH_NULL, PUSH_BOOL, PUSH_INT, PUSH_FLOAT, PUSH_STRING, POP, DUP
// Arithmetic: ADD, SUB, MUL, DIV, MOD (5)
// Comparison: EQ, NEQ, LT, LTE, GT, GTE (6)
// Logic: AND, OR, NOT (3)
// Control: JMP, JMPT, JMPF, CALL, RET (5)
// Variables: LOAD_LOCAL, STORE_LOCAL, LOAD_GLOBAL, STORE_GLOBAL (4)
// Arrays: ARRAY_NEW, ARRAY_GET, ARRAY_SET, ARRAY_LEN (4)
// Objects: OBJECT_NEW, OBJECT_GET, OBJECT_SET (3)
// Exceptions: TRY_START, CATCH_START, CATCH_END, THROW (4)
// Closures: MAKE_CLOSURE, LOAD_UPVALUE, STORE_UPVALUE (3)
// Other: HALT, NOP (2)
```

---

## 8️⃣ Garbage Collector 모듈 (`include/gc.h`, `src/gc.c`)

**라인**: 361줄 헤더 + 913줄 소스 = 1,274줄
**함수**: 28개
**알고리즘**: Mark-and-sweep

### 핵심 구조
```c
typedef struct {
    fl_value_t** objects;
    size_t count;
    size_t capacity;
    size_t threshold;           // GC trigger threshold
} fl_gc_t;
```

### 함수
```c
fl_gc_t* fl_gc_create(void);
void fl_gc_destroy(fl_gc_t* gc);
void fl_gc_collect(fl_gc_t* gc);      // Mark-sweep 실행

// Marking
void fl_gc_mark(fl_gc_t* gc, fl_value_t* root);
int fl_gc_is_marked(fl_value_t* value);

// Sweeping
void fl_gc_sweep(fl_gc_t* gc);
void fl_gc_free_unmarked(fl_gc_t* gc);

// Memory tracking
void fl_gc_track_array(fl_gc_t* gc, fl_array_t* array);
void fl_gc_track_object(fl_gc_t* gc, fl_object_t* object);
void fl_gc_track_function(fl_gc_t* gc, fl_function_t* func);
void fl_gc_track_closure(fl_gc_t* gc, fl_closure_t* closure);

// Statistics
size_t fl_gc_stats(fl_gc_t* gc);
// ... 15+ 더 많은 함수
```

---

## 9️⃣ Closure 모듈 (`include/closure.h`, `src/closure.c`)

**라인**: 104줄 헤더 + 215줄 소스 = 319줄
**함수**: 11개

### 핵심 구조
```c
typedef struct {
    char* name;
    fl_value_t value;
} fl_captured_var_t;

typedef struct fl_closure {
    fl_function_t* func;        // 기본 함수
    
    fl_captured_var_t* captured_vars;
    size_t captured_count;
    size_t captured_capacity;
} fl_closure_t;
```

### 함수
```c
fl_closure_t* fl_closure_create(fl_function_t* func);
void fl_closure_free(fl_closure_t* closure);

void fl_closure_capture_var(fl_closure_t* closure, const char* name, fl_value_t value);
fl_value_t fl_closure_get_upvalue(fl_closure_t* closure, const char* name);
void fl_closure_set_upvalue(fl_closure_t* closure, const char* name, fl_value_t value);

int fl_closure_has_var(fl_closure_t* closure, const char* name);
fl_value_t fl_closure_call(fl_closure_t* closure, fl_value_t* args, size_t arg_count);
// ... 4+ 더 많은 함수
```

---

## 🔟 Standard Library 모듈 (`include/stdlib_fl.h`, `src/stdlib.c`)

**라인**: 92줄 헤더 + 1,737줄 소스 = 1,829줄
**함수**: 76개

### 함수 카테고리
```
Type/Conversion (8):        typeof, toNumber, toString, toBoolean, ...
Math (12):                  add, sub, mul, div, mod, pow, sqrt, sin, cos, ...
String (14):                strlen, strcat, substr, indexOf, split, trim, ...
Array (15):                 push, pop, shift, unshift, slice, join, reverse, sort, ...
Object (8):                 keys, values, entries, hasKey, delete, ...
File I/O (6):               readFile, writeFile, appendFile, ...
Cryptography (7):           md5, sha256, base64_encode, base64_decode, ...
Utilities (6):              print, assert, random, sleep, ...
```

### 함수
```c
void fl_stdlib_register_all(void);

// Type functions
fl_value_t fl_typeof(fl_value_t value);
fl_value_t fl_toNumber(fl_value_t value);
fl_value_t fl_toString(fl_value_t value);
fl_value_t fl_toBoolean(fl_value_t value);

// Math functions
fl_value_t fl_add(fl_value_t a, fl_value_t b);
fl_value_t fl_sub(fl_value_t a, fl_value_t b);
fl_value_t fl_mul(fl_value_t a, fl_value_t b);
fl_value_t fl_div(fl_value_t a, fl_value_t b);
fl_value_t fl_mod(fl_value_t a, fl_value_t b);
fl_value_t fl_pow(fl_value_t base, fl_value_t exp);
fl_value_t fl_sqrt(fl_value_t value);
// ... 60+ 더 많은 함수
```

---

## 1️⃣1️⃣ Runtime 모듈 (`include/runtime.h`, `src/runtime.c`)

**라인**: 79줄 헤더 + 619줄 소스 = 698줄
**함수**: 11개

### 핵심 구조
```c
typedef struct fl_runtime {
    fl_vm_t* vm;
    fl_parser_t* parser;
    fl_compiler_t* compiler;
    fl_gc_t* gc;
    
    // User-defined functions
    void* functions;            // HashMap<string, fl_function_t*>
} fl_runtime_t;
```

### 함수
```c
fl_runtime_t* fl_runtime_create(void);
void fl_runtime_destroy(fl_runtime_t* runtime);
fl_value_t fl_runtime_eval(fl_runtime_t* runtime, const char* source);

void fl_runtime_register_function(fl_runtime_t* runtime, const char* name, 
                                   fl_function_t* func);
fl_function_t* fl_runtime_get_function(fl_runtime_t* runtime, const char* name);

void fl_runtime_set_global(fl_runtime_t* runtime, const char* name, fl_value_t value);
fl_value_t fl_runtime_get_global(fl_runtime_t* runtime, const char* name);

int fl_runtime_execute_file(fl_runtime_t* runtime, const char* filename);
// ... 3+ 더 많은 함수
```

---

## 1️⃣2️⃣ Error 모듈 (`include/error.h`, `src/error.c`)

**라인**: 164줄 헤더 + 466줄 소스 = 630줄
**함수**: 24개

### 핵심 구조
```c
typedef enum {
    FL_ERR_NONE, FL_ERR_SYNTAX, FL_ERR_RUNTIME, FL_ERR_TYPE,
    FL_ERR_REFERENCE, FL_ERR_INDEX_OUT_OF_BOUNDS, FL_ERR_DIVIDE_BY_ZERO,
    FL_ERR_UNDEFINED_VARIABLE, FL_ERR_UNDEFINED_FUNCTION, FL_ERR_INVALID_ARGUMENT,
    FL_ERR_ASSERTION_FAILED, FL_ERR_IO, FL_ERR_CUSTOM
} fl_error_type_t;

typedef struct fl_error {
    fl_error_type_t type;
    char* message;
    char* filename;
    int line;
    int column;
    char* stack_trace;
} fl_error_t;
```

### 함수
```c
fl_error_t* fl_error_create(fl_error_type_t type, const char* message, 
                             const char* filename, int line, int column);
void fl_error_free(fl_error_t* error);

void fl_error_print(fl_error_t* error);
void fl_error_set_stack_trace(fl_error_t* error, const char* trace);

const char* fl_error_type_name(fl_error_type_t type);
void fl_error_handle(fl_error_t* error);

// Exception handling
void fl_try_push_handler(size_t handler_addr);
void fl_catch_pop_handler(void);
void fl_throw_exception(fl_value_t value);
// ... 10+ 더 많은 함수
```

---

## 📊 모듈 간 데이터 흐름

```
Lexer:
  Input: 소스 코드 (char*)
  Output: Token 배열

Parser:
  Input: Token 배열 (from Lexer)
  Output: AST (fl_ast_node_t*)

Type Checker:
  Input: AST (from Parser)
  Output: Type-annotated AST

Compiler:
  Input: Type-annotated AST (from Type Checker)
  Output: Bytecode Chunk (code + constants)

VM:
  Input: Bytecode Chunk (from Compiler)
  Helper: GC, Runtime, Stdlib, Closure
  Output: fl_value_t (9 types)

GC (Background):
  Tracks: Arrays, Objects, Functions, Closures
  Algorithm: Mark-and-sweep

Runtime:
  Manages: Lexer, Parser, Compiler, VM, GC
  Provides: eval(), file execution

Stdlib:
  Registered: VM execution context
  Functions: 76개 내장 함수
```

---

## 🔄 호출 순서 (완전 실행)

```
1. fl_runtime_create()
   ├─ fl_vm_create()
   ├─ fl_gc_create()
   ├─ fl_parser_create() stub
   └─ fl_compiler_new() stub

2. fl_runtime_eval(runtime, source_code)
   ├─ fl_lexer_create(source)
   ├─ lexer_scan_all()  → Token[]
   ├─ fl_parser_create(tokens)
   ├─ fl_parser_parse()  → AST
   ├─ (type_check AST)   → Type-annotated AST
   ├─ compile_program()  → Chunk
   ├─ fl_vm_execute(chunk)
   │  ├─ Call PUSH opcodes → Stack
   │  ├─ Call arithmetic opcodes → Stack operations
   │  ├─ Call control flow opcodes → Jump/Call
   │  ├─ Call variable opcodes → Local/Global access
   │  ├─ Exception handling if needed
   │  └─ Return fl_value_t
   └─ fl_gc_collect() if needed

3. fl_runtime_destroy()
   ├─ fl_vm_destroy()
   ├─ fl_gc_destroy()
   ├─ fl_lexer_destroy()
   └─ Free all allocated memory
```

---

## ✅ 설계 원칙

1. **모듈화**: 각 모듈이 독립적 책임
2. **메모리 관리**: 모든 malloc에 대응하는 free
3. **에러 처리**: fl_error_t 구조로 일관된 처리
4. **확장성**: 새 opcode/함수/타입 추가 용이
5. **디버깅**: 라인 맵핑, 에러 메시지, print 함수
6. **성능**: 객체 풀, 동적 배열, 해시맵 사용

