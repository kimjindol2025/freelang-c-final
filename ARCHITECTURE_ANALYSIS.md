# FreeLang C Runtime — 완전 아키텍처 분석 (2026-03-06)

**프로젝트**: `/home/kimjin/Desktop/kim/freelang-c/`
**총 코드**: 12,473줄 (Header 2,628 + Source 9,845)
**모듈**: 13개 (Lexer, Parser, AST, Compiler, VM, GC, Type Checker, Runtime, Error, Stdlib, Token, Closure)
**총 함수**: ~330개 (추정)

---

## 📊 코드 분포 (12,473줄)

### Header Files (2,628줄)
```
  586  include/ast.h              (AST 노드 47개 타입)
  575  include/typechecker.h      (타입 검증)
  361  include/gc.h               (GC 알고리즘)
  254  include/freelang.h         (메인 헤더)
  164  include/error.h            (에러 처리)
  158  include/compiler.h         (컴파일러 인터페이스)
  115  include/token.h            (토큰 정의 62개)
  104  include/closure.h          (클로저 구조)
   92  include/stdlib_fl.h        (표준 라이브러리)
   79  include/runtime.h          (런타임 인터페이스)
   68  include/vm.h               (VM 인터페이스)
   38  include/lexer.h            (렉서 인터페이스)
   34  include/parser.h           (파서 인터페이스)
```

### Source Files (9,845줄)
```
  1,737  src/stdlib.c            (76개 표준 함수)
  1,312  src/compiler.c          (26개 컴파일 함수)
  1,245  src/vm.c                (VM 실행 엔진)
    973  src/parser.c            (40개 파싱 함수)
    945  src/typechecker.c       (49개 타입 검증)
    913  src/gc.c                (28개 GC 함수)
    619  src/runtime.c           (11개 런타임 함수)
    606  src/ast.c               (29개 AST 함수)
    499  src/lexer.c             (18개 렉싱 함수)
    466  src/error.c             (24개 에러 처리)
    215  src/closure.c           (11개 클로저 함수)
    186  src/main.c              (진입점)
    129  src/token.c             (4개 토큰 함수)
```

---

## 🔴 핵심 아키텍처 다이어그램

```
┌─────────────────────────────────────────────────────────────────┐
│                          SOURCE CODE                             │
│                    (FreeLang .fl file)                          │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
        ┌─────────────────────────────────┐
        │  LEXER (lexer.c, 499줄)         │
        │  • Tokenization: 62개 토큰     │
        │  • Keywords: 28개              │
        │  • Operators: 26개             │
        │  • Punctuation: 13개           │
        │  • Keyword HashMap lookup      │
        └────────────┬────────────────────┘
                     │ Token Stream
                     ▼
        ┌─────────────────────────────────┐
        │  PARSER (parser.c, 973줄)       │
        │  • 40개 파싱 함수               │
        │  • Recursive descent parser      │
        │  • Expression precedence        │
        │  • Statement/declaration        │
        └────────────┬────────────────────┘
                     │ AST
                     ▼
        ┌─────────────────────────────────┐
        │  TYPE CHECKER (typechecker.c)   │
        │  (945줄, 49개 함수)             │
        │  • Type inference               │
        │  • Type compatibility check     │
        │  • Generic type support         │
        │  • Error reporting              │
        └────────────┬────────────────────┘
                     │ Validated AST
                     ▼
        ┌─────────────────────────────────┐
        │  COMPILER (compiler.c, 1,312줄) │
        │  • 26개 컴파일 함수              │
        │  • AST → Bytecode conversion    │
        │  • Chunk 생성 (code + consts)  │
        │  • Local variable tracking      │
        │  • Break/continue patching      │
        │  • Closure capture analysis     │
        └────────────┬────────────────────┘
                     │ Bytecode Chunk
                     ▼
        ┌─────────────────────────────────┐
        │  VM EXECUTION (vm.c, 1,245줄)   │
        │  • Stack: 10,000 slots         │
        │  • Frames: 256 depth           │
        │  • 45개 Opcode 실행            │
        │  • Call frames 관리            │
        │  • Exception handling          │
        │  • Closure context             │
        └────────────┬────────────────────┘
                     │
    ┌────────────────┼────────────────┐
    │                │                │
    ▼                ▼                ▼
┌──────────┐  ┌──────────┐  ┌──────────────┐
│ STDLIB   │  │ GC       │  │ RUNTIME      │
│ (1,737줄)│  │(913줄)   │  │(619줄)       │
│ 76 func  │  │28 func   │  │11 func       │
└──────────┘  └──────────┘  └──────────────┘
    │              │              │
    └──────────────┼──────────────┘
                   │
                   ▼
        ┌─────────────────────────────────┐
        │     RESULT (fl_value_t)         │
        │  9 Value Types:                │
        │  • NULL, BOOL, INT, FLOAT      │
        │  • STRING, ARRAY, OBJECT       │
        │  • FUNCTION, CLOSURE, ERROR    │
        └─────────────────────────────────┘
```

---

## 🔵 렉서 (Lexer) 구조

**파일**: `src/lexer.c` (499줄), `include/lexer.h` (38줄)
**함수**: 18개

### 토큰 정의 (include/token.h)

**Literals (4개)**:
```c
TOK_NUMBER, TOK_STRING, TOK_FSTRING, TOK_IDENT
```

**Keywords (28개)**:
```c
TOK_LET, TOK_CONST, TOK_VAR, TOK_FN,
TOK_IF, TOK_ELSE, TOK_WHILE, TOK_FOR, TOK_IN,
TOK_RETURN, TOK_BREAK, TOK_CONTINUE,
TOK_TRUE, TOK_FALSE, TOK_NULL,
TOK_STRUCT, TOK_ENUM,
TOK_TRY, TOK_CATCH, TOK_FINALLY, TOK_THROW,
TOK_IMPORT, TOK_FROM, TOK_EXPORT, TOK_DEFAULT, TOK_AS,
TOK_ASYNC, TOK_AWAIT, TOK_MATCH
```

**Operators (26개)**:
```c
TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT, TOK_POWER,
TOK_EQ, TOK_EQEQ, TOK_EQEQEQ, TOK_NOT, TOK_NOTEQ, TOK_NOTEQUALEQ,
TOK_LT, TOK_LTEQ, TOK_GT, TOK_GTEQ,
TOK_AND, TOK_OR, TOK_AMPERSAND, TOK_PIPE, TOK_CARET, TOK_TILDE,
TOK_LSHIFT, TOK_RSHIFT, TOK_ARROW, TOK_QUESTION
```

**Punctuation (13개)**:
```c
TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE, TOK_LBRACKET, TOK_RBRACKET,
TOK_COMMA, TOK_DOT, TOK_SEMICOLON, TOK_COLON, TOK_AT, TOK_HASH, TOK_DOLLAR
```

**Compound assignments (4개)**:
```c
TOK_PLUS_EQ, TOK_MINUS_EQ, TOK_STAR_EQ, TOK_SLASH_EQ
```

**Special (2개)**:
```c
TOK_NEWLINE, TOK_EOF
```

**총 토큰 타입**: 62개

### Lexer 구조
```c
typedef struct {
    const char *src;
    size_t src_len;
    size_t pos;
    int line, col;
    
    Token *tokens;              // 동적 배열
    size_t token_count;
    size_t token_capacity;
    
    void *keyword_map;          // HashMap<string, TokenType>
} Lexer;
```

---

## 🟢 파서 (Parser) 구조

**파일**: `src/parser.c` (973줄), `include/parser.h` (34줄)
**함수**: 40개
**파싱 방식**: Recursive descent

### AST 노드 타입 (47개)

```c
// Program & Module
NODE_PROGRAM

// Declarations (4개)
NODE_VAR_DECL, NODE_FN_DECL, NODE_STRUCT_DECL, NODE_ENUM_DECL

// Module System (2개)
NODE_IMPORT, NODE_EXPORT

// Statements (10개)
NODE_BLOCK, NODE_IF, NODE_WHILE, NODE_FOR, NODE_FOR_IN,
NODE_RETURN, NODE_BREAK, NODE_CONTINUE, NODE_TRY, NODE_THROW

// Expressions (7개)
NODE_BINARY, NODE_UNARY, NODE_LOGICAL, NODE_CALL, NODE_MEMBER, NODE_ASSIGN, NODE_TERNARY

// Literals & Collections (3개)
NODE_ARRAY, NODE_OBJECT, NODE_PROPERTY

// Advanced Expressions (5개)
NODE_FN_EXPR, NODE_ARROW_FN, NODE_AWAIT, NODE_FSTRING, NODE_QUESTION_OP

// Identifiers & Literals (2개)
NODE_IDENT, NODE_LITERAL

// Clauses (2개)
NODE_CATCH_CLAUSE, NODE_FINALLY_CLAUSE
```

### AST 노드 구조 (union, ~600 bytes)
```c
typedef struct fl_ast_node {
    NodeType type;
    int line, col;
    TypeAnnotation *type_info;
    
    union {
        // VAR_DECL: let x = init;
        struct {
            char kind[8];           // "let", "const", "var"
            char name[256];
            struct fl_ast_node *init;
            TypeAnnotation *type;
        } var_decl;
        
        // FN_DECL: fn name(params) -> Type { body }
        struct {
            char name[256];
            char **param_names;
            int param_count;
            TypeAnnotation **param_types;
            TypeAnnotation *return_type;
            struct fl_ast_node *body;
            int is_async;
            int is_generic;
            char **generic_params;
            int generic_param_count;
        } fn_decl;
        
        // ... 45개 더 많은 노드 타입 ...
    } data;
} fl_ast_node_t;
```

---

## 🟡 컴파일러 (Compiler) 구조

**파일**: `src/compiler.c` (1,312줄), `include/compiler.h` (158줄)
**함수**: 26개
**목표**: AST → Bytecode (Chunk)

### Chunk 구조
```c
typedef struct {
    uint8_t *code;              // Bytecode array
    size_t code_len;
    size_t code_capacity;
    
    fl_value_t *consts;         // Constants pool (string, numbers)
    size_t const_len;
    size_t const_capacity;
    
    int locals_count;           // Local variable count
    int *line_map;              // Debugging: line number mapping
    size_t line_map_capacity;
} Chunk;
```

### Compiler 상태
```c
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
    
    // Scope & closure capture
    int scope_level;            // 0=module, 1+=nested
    CapturedVarInfo captured_vars[256];
    int captured_count;
    
    // Functions table
    fl_function_t *functions[256];
    int function_count;
} Compiler;
```

### 컴파일 프로세스
```
AST 노드
  ↓
1. compile_program() - 프로그램 전체
2. compile_stmt() - 각 문장
3. compile_expr() - 각 표현식
4. chunk_emit_opcode() - 바이트코드 발생
5. chunk_emit_int/float/string() - 상수 저장
6. chunk_patch_jump() - 조건부 점프 패칭
  ↓
Bytecode Chunk (code + constants)
```

---

## 🔴 VM (Virtual Machine) 구조

**파일**: `src/vm.c` (1,245줄), `include/vm.h` (68줄)
**함수**: 19개
**아키텍처**: **Stack-based bytecode VM**

### VM 상태
```c
typedef struct fl_vm {
    fl_value_t stack[FL_VM_STACK_SIZE];  // 스택: 10,000 슬롯
    size_t stack_top;
    
    fl_call_frame_t frames[FL_VM_FRAME_SIZE];  // 콜 프레임: 256 깊이
    size_t frame_count;
    
    fl_value_t locals[256];             // 로컬 변수 (현재 스코프)
    size_t local_count;
    
    fl_value_t globals;                 // 전역 변수 (객체)
    void* gc;                           // GC 참조
    
    void* runtime;                      // fl_runtime_t* (사용자 함수)
    
    // 예외 처리
    fl_value_t exception;
    bool exception_active;
    size_t exception_handler;
    
    // 클로저 컨텍스트
    struct fl_closure* current_closure;
} fl_vm_t;
```

### Call Frame 구조
```c
typedef struct fl_call_frame {
    uint8_t* ip;                        // Instruction pointer
    fl_value_t* locals;                 // 스택 프레임의 로컬
    size_t local_count;
    struct fl_closure* closure;         // 클로저 컨텍스트 (if any)
} fl_call_frame_t;
```

### VM 성능
```
Stack size:     10,000 slots
Frame depth:    256 levels
Locals per frame: 256 variables
Max recursion:  256 levels
```

---

## 🟠 Opcode 정의 (45개)

### Stack Operations (7개)
```c
FL_OP_PUSH_NULL        // 스택에 null 푸시
FL_OP_PUSH_BOOL        // 부울 값 푸시 (operand: 0 or 1)
FL_OP_PUSH_INT         // 정수 상수 푸시 (4-byte addr)
FL_OP_PUSH_FLOAT       // 부동소수점 상수 푸시 (4-byte addr)
FL_OP_PUSH_STRING      // 문자열 상수 푸시 (4-byte addr)
FL_OP_POP              // 스택 팝
FL_OP_DUP              // 스택 top 복제
```

### Arithmetic (5개)
```c
FL_OP_ADD              // pop B, pop A, push A+B
FL_OP_SUB              // pop B, pop A, push A-B
FL_OP_MUL              // pop B, pop A, push A*B
FL_OP_DIV              // pop B, pop A, push A/B
FL_OP_MOD              // pop B, pop A, push A%B
```

### Comparison (6개)
```c
FL_OP_EQ               // A == B
FL_OP_NEQ              // A != B
FL_OP_LT               // A < B
FL_OP_LTE              // A <= B
FL_OP_GT               // A > B
FL_OP_GTE              // A >= B
```

### Logic (3개)
```c
FL_OP_AND              // A && B (short-circuit)
FL_OP_OR               // A || B (short-circuit)
FL_OP_NOT              // !A
```

### Control Flow (5개)
```c
FL_OP_JMP              // Unconditional jump (4-byte addr)
FL_OP_JMPT             // Jump if true (4-byte addr)
FL_OP_JMPF             // Jump if false (4-byte addr)
FL_OP_CALL             // Call function (operand: arg count)
FL_OP_RET              // Return from function
```

### Variables (4개)
```c
FL_OP_LOAD_LOCAL       // 로컬 변수 로드 (operand: index)
FL_OP_STORE_LOCAL      // 로컬 변수 저장 (operand: index)
FL_OP_LOAD_GLOBAL      // 전역 변수 로드 (4-byte string addr)
FL_OP_STORE_GLOBAL     // 전역 변수 저장 (4-byte string addr)
```

### Arrays (4개)
```c
FL_OP_ARRAY_NEW        // 빈 배열 생성 (operand: size)
FL_OP_ARRAY_GET        // 배열 요소 로드 [idx]
FL_OP_ARRAY_SET        // 배열 요소 저장 [idx] = val
FL_OP_ARRAY_LEN        // 배열 길이
```

### Objects (3개)
```c
FL_OP_OBJECT_NEW       // 빈 객체 생성
FL_OP_OBJECT_GET       // 객체 속성 로드 .prop
FL_OP_OBJECT_SET       // 객체 속성 저장 .prop = val
```

### Exceptions (4개)
```c
FL_OP_TRY_START        // try 블록 시작
FL_OP_CATCH_START      // catch 블록 시작
FL_OP_CATCH_END        // catch 블록 끝
FL_OP_THROW            // 예외 던지기
```

### Closures (3개) - NEW
```c
FL_OP_MAKE_CLOSURE     // 함수 + 캡처된 변수로 클로저 생성
FL_OP_LOAD_UPVALUE     // 캡처된 변수 로드
FL_OP_STORE_UPVALUE    // 캡처된 변수 저장
```

### Other (2개)
```c
FL_OP_HALT             // VM 종료
FL_OP_NOP              // No operation
```

**총 45개 Opcode**

---

## 🟣 타입 시스템

**파일**: `src/typechecker.c` (945줄), `include/typechecker.h` (575줄)
**함수**: 49개

### Value Types (9개)
```c
typedef enum {
    FL_TYPE_NULL,           // null
    FL_TYPE_BOOL,           // true, false
    FL_TYPE_INT,            // 64-bit signed
    FL_TYPE_FLOAT,          // IEEE 754 double
    FL_TYPE_STRING,         // UTF-8 string
    FL_TYPE_ARRAY,          // Dynamic array
    FL_TYPE_OBJECT,         // Key-value map
    FL_TYPE_FUNCTION,       // User-defined function
    FL_TYPE_CLOSURE,        // Function + captured environment (NEW)
    FL_TYPE_ERROR           // Error/Exception
} fl_type_t;
```

### Value Union
```c
typedef struct fl_value {
    fl_type_t type;
    union {
        fl_bool bool_val;
        fl_int int_val;            // int64_t
        fl_float float_val;        // double
        fl_string string_val;
        struct fl_array* array_val;
        struct fl_object* object_val;
        struct fl_function* func_val;
        struct fl_closure* closure_val;     // NEW
        struct fl_error* error_val;
    } data;
} fl_value_t;
```

### Error Types (13개)
```c
FL_ERR_NONE
FL_ERR_SYNTAX
FL_ERR_RUNTIME
FL_ERR_TYPE
FL_ERR_REFERENCE
FL_ERR_INDEX_OUT_OF_BOUNDS
FL_ERR_DIVIDE_BY_ZERO
FL_ERR_UNDEFINED_VARIABLE
FL_ERR_UNDEFINED_FUNCTION
FL_ERR_INVALID_ARGUMENT
FL_ERR_ASSERTION_FAILED
FL_ERR_IO
FL_ERR_CUSTOM
```

---

## 🟢 메모리 관리 (GC)

**파일**: `src/gc.c` (913줄), `include/gc.h` (361줄)
**함수**: 28개
**알고리즘**: Mark-and-sweep

### GC 구조
```c
typedef struct {
    fl_value_t** objects;       // Tracked objects
    size_t count;
    size_t capacity;
    size_t threshold;           // GC trigger point
} fl_gc_t;
```

### GC 함수
```
gc_collect() - Mark-sweep 실행
gc_mark() - 도달 가능한 객체 표시
gc_sweep() - 미표시 객체 정리
```

---

## 🔵 클로저 (Closure)

**파일**: `src/closure.c` (215줄), `include/closure.h` (104줄)
**함수**: 11개

### Closure 구조
```c
typedef struct {
    char* name;             // Variable name
    fl_value_t value;       // Captured value
} fl_captured_var_t;

typedef struct fl_closure {
    fl_function_t* func;    // 기본 함수
    
    // 캡처된 변수 (렉시컬 환경)
    fl_captured_var_t* captured_vars;
    size_t captured_count;
    size_t captured_capacity;
} fl_closure_t;
```

### 클로저 지원
- Function expressions: `let f = fn(x) { return x*2; }`
- Arrow functions: `(x) => x*2`
- 고차함수: `map(arr, fn)`
- 클로저 capture analysis (컴파일러)
- Upvalue load/store (VM)

---

## 🟡 표준 라이브러리

**파일**: `src/stdlib.c` (1,737줄), `include/stdlib_fl.h` (92줄)
**함수**: 76개

### 카테고리
- **타입/변환**: typeof, toNumber, toString, toBoolean
- **수학**: add, sub, mul, div, mod, pow, sqrt, sin, cos, etc.
- **문자열**: strlen, strcat, substr, indexOf, split, trim, etc.
- **배열**: push, pop, shift, unshift, slice, join, reverse, sort
- **객체**: keys, values, entries, hasKey, delete
- **파일I/O**: readFile, writeFile, appendFile
- **암호화**: md5, sha256, base64_encode, base64_decode
- **네트워킹**: (예비)

---

## 📈 모듈 간 의존성

```
main.c
  ├─→ runtime.c
  │     ├─→ parser.c
  │     ├─→ compiler.c
  │     ├─→ vm.c
  │     └─→ gc.c
  └─→ stdlib.c (자동 등록)

parser.c
  ├─→ lexer.c
  ├─→ ast.c
  └─→ error.c

compiler.c
  ├─→ ast.c
  ├─→ vm.c (opcode 정의)
  ├─→ closure.c (capture analysis)
  └─→ error.c

vm.c
  ├─→ stdlib.c (내장 함수)
  ├─→ gc.c (메모리 관리)
  ├─→ closure.c (클로저 실행)
  ├─→ runtime.c (사용자 함수)
  └─→ error.c (예외 처리)

typechecker.c
  ├─→ ast.c
  └─→ error.c
```

---

## 🎯 v4-core와의 매핑 가능성

### 재사용 가능 패턴
| 항목 | C 구현 | v4-core TypeScript | 상태 |
|------|--------|-------------------|------|
| Lexer | 499줄 (18함수) | 필요 | ✅ 재구현 가능 |
| Parser | 973줄 (40함수) | 필요 | ✅ 구조 참조 |
| AST | 47노드 타입 | 필요 | ✅ 노드 정의 참조 |
| Type System | 9 타입 + 13 에러 | 필요 | ✅ 타입 참조 |
| Compiler | 1,312줄 (26함수) | 필요 | ✅ 알고리즘 참조 |
| VM | Stack-based, 45 opcode | 필요 | ✅ 아키텍처 참조 |
| Closure | 11함수, capture analysis | 필요 | ✅ 구현 참조 |
| GC | Mark-sweep (913줄) | 필요 | ✅ 알고리즘 참조 |
| Stdlib | 76함수 (1,737줄) | 필요 | ✅ 함수 목록 참조 |

### 권장 마이그레이션 순서
1. **Lexer** (499줄) → TypeScript + Regex/state machine
2. **Token 정의** (62개) → Enum
3. **AST 노드** (47개) → Type unions
4. **Parser** (973줄) → Recursive descent (TypeScript)
5. **Type System** (9 타입) → Type aliases
6. **Compiler** (1,312줄) → AST → Bytecode
7. **VM** (1,245줄) → Stack-based execution
8. **GC** (913줄) → Reference counting 또는 Mark-sweep
9. **Stdlib** (76함수) → Native functions
10. **Closure support** (215줄) → Upvalue capture

---

## 📊 통계 요약

| 항목 | 수치 |
|------|------|
| **총 코드** | 12,473줄 |
| **Header** | 2,628줄 (13파일) |
| **Source** | 9,845줄 (13파일) |
| **총 함수** | ~330개 |
| **AST 노드 타입** | 47개 |
| **Token 타입** | 62개 |
| **Opcode** | 45개 |
| **Value 타입** | 9개 |
| **Error 타입** | 13개 |
| **표준 함수** | 76개 |
| **VM Stack** | 10,000 슬롯 |
| **Call Frame Depth** | 256 레벨 |
| **최대 로컬 변수** | 256개 |

---

## ✅ 아키텍처 특징

✅ **완전히 독립적**: 0 외부 의존성 (libc만 사용)
✅ **타입 안전**: 정적 타입 검사기 내장
✅ **메모리 관리**: GC 내장 (Mark-sweep)
✅ **예외 처리**: try-catch-finally 지원
✅ **클로저 지원**: 변수 capture + upvalue
✅ **함수형 지원**: 고차함수, 람다, 화살표 함수
✅ **모듈 시스템**: import/export 지원
✅ **확장 가능**: 표준 함수 쉽게 추가
✅ **디버깅**: 라인 맵핑, 에러 보고

