# FreeLang C AST (Abstract Syntax Tree) Implementation Guide

## 📋 Overview

FreeLang C의 AST 모듈은 프로그래밍 언어의 구조를 메모리에 표현합니다. 이 문서는 AST 설계, 구현, 사용법을 설명합니다.

**파일 위치**: `/home/kimjin/Desktop/kim/freelang-c/include/ast.h`, `src/ast.c`

**크기**: 약 650줄 (헤더) + 650줄 (구현)

---

## 🏗️ Architecture

### 1. NodeType Enum (35개 노드 타입)

```c
typedef enum {
    /* Program & Module */
    NODE_PROGRAM,

    /* Declarations */
    NODE_VAR_DECL,      // let/const/var x = init
    NODE_FN_DECL,       // fn name(params) { body }
    NODE_STRUCT_DECL,   // struct Name { fields }
    NODE_ENUM_DECL,     // enum Name { variants }

    /* Module System */
    NODE_IMPORT,        // import x from "module"
    NODE_EXPORT,        // export let x = 10

    /* Statements */
    NODE_BLOCK,         // { stmt1; stmt2; ... }
    NODE_IF,            // if (test) then else
    NODE_WHILE,         // while (test) body
    NODE_FOR,           // for (init; test; update) body
    NODE_FOR_IN,        // for (x in array) body
    NODE_RETURN,        // return value
    NODE_BREAK,         // break
    NODE_CONTINUE,      // continue
    NODE_TRY,           // try-catch-finally
    NODE_THROW,         // throw error

    /* Expressions */
    NODE_BINARY,        // left + right
    NODE_UNARY,         // -x, !x
    NODE_LOGICAL,       // x && y, x || y
    NODE_CALL,          // f(a, b)
    NODE_MEMBER,        // obj.prop, arr[idx]
    NODE_ASSIGN,        // x = value
    NODE_TERNARY,       // test ? then : else

    /* Collections */
    NODE_ARRAY,         // [1, 2, 3]
    NODE_OBJECT,        // { key: val }
    NODE_PROPERTY,      // key: value (in object)

    /* Advanced */
    NODE_ARROW_FN,      // (x) => x * 2
    NODE_AWAIT,         // await promise
    NODE_FSTRING,       // `Hello ${name}`
    NODE_QUESTION_OP,   // obj?.prop

    /* Leaf Nodes */
    NODE_IDENT,         // variable names
    NODE_LITERAL,       // 42, "hello", true, null

    /* Exception Handling */
    NODE_CATCH_CLAUSE,  // catch (e) { handler }
    NODE_FINALLY_CLAUSE,// finally { handler }

    NODE_TYPE_MAX
} NodeType;
```

### 2. ASTNode 구조체 (Union 방식)

```c
typedef struct ASTNode {
    NodeType type;
    int line, col;
    TypeAnnotation *type_info;

    union {
        struct { /* NODE_PROGRAM */ } program;
        struct { /* NODE_BINARY */ } binary;
        struct { /* NODE_VAR_DECL */ } var_decl;
        struct { /* NODE_CALL */ } call;
        /* ... 나머지 32개 노드 타입 ... */
    } data;
} ASTNode;
```

**이점**:
- 메모리 효율적 (각 노드는 최대 타입별 데이터만 차지)
- 타입 안전성 (NodeType으로 union 필드 구분)
- 캐시 친화적 (ASTNode 크기 일정)

---

## 💾 Object Pool Allocator

```c
typedef struct {
    ASTNode *nodes;         /* 할당된 노드 배열 */
    int *is_used;           /* 사용 여부 플래그 */
    size_t capacity;        /* 최대 용량 */
    size_t count;           /* 현재 사용 중인 노드 수 */
} ASTNodePool;
```

### 초기화

```c
ast_init_pool(10000);  /* 10,000개 노드 미리 할당 */
```

### 할당 & 해제

```c
ASTNode *node = ast_alloc(NODE_BINARY);
ast_free(node);         /* 단일 노드 해제 */
ast_free_all();         /* 모든 노드 해제 */
```

**왜 Object Pool인가?**
1. **성능**: malloc/free 반복 오버헤드 감소
2. **메모리**: 재사용 가능한 슬롯 재활용
3. **캐시**: 할당된 메모리 위치 예측 가능
4. **수명 관리**: 파싱 끝까지 모든 노드 유지 가능

---

## 🔧 Node 타입별 구조

### 1. Literals (NODE_LITERAL)

```c
struct {
    double num;        /* 숫자 값 */
    char *str;         /* 문자열 포인터 */
    int is_bool;       /* 부울 플래그 */
    int bool_val;      /* true=1, false=0 */
    int is_null;       /* null 플래그 */
    int is_nan;        /* NaN 플래그 */
    int is_inf;        /* Infinity 플래그 */
} literal;
```

**사용 예**:
```c
ASTNode *num = ast_new_number(42.5);
ASTNode *str = ast_new_string("hello");
ASTNode *bool_val = ast_new_bool(1);
ASTNode *null_val = ast_new_null();
```

### 2. Binary Expressions (NODE_BINARY)

```c
struct {
    ASTNode *left;
    char op[16];       /* "+", "-", "*", "/", "==", "!=", etc */
    ASTNode *right;
} binary;
```

**사용 예**:
```c
ASTNode *expr = ast_new_binary(
    ast_new_number(5),
    "+",
    ast_new_number(3)
);  /* 5 + 3 */
```

### 3. Variable Declaration (NODE_VAR_DECL)

```c
struct {
    char kind[8];           /* "let", "const", "var" */
    char name[256];
    ASTNode *init;          /* nullable */
    TypeAnnotation *type;   /* optional */
} var_decl;
```

**사용 예**:
```c
ASTNode *var = ast_new_var_decl("let", "x", ast_new_number(10));
/* let x = 10; */
```

### 4. Function Declaration (NODE_FN_DECL)

```c
struct {
    char name[256];
    char **param_names;     /* ["x", "y", "z"] */
    int param_count;
    TypeAnnotation **param_types;  /* optional */
    TypeAnnotation *return_type;   /* optional */
    ASTNode *body;          /* BlockStatement */
    int is_async;           /* async fn */
    int is_generic;         /* fn<T> */
    char **generic_params;  /* ["T", "U"] */
    int generic_param_count;
} fn_decl;
```

### 5. Function Call (NODE_CALL)

```c
struct {
    ASTNode *callee;
    ASTNode **arguments;    /* 함수 인자 배열 */
    int arg_count;
} call;
```

**사용 예**:
```c
ASTNode *f = ast_new_ident("println");
ASTNode *arg = ast_new_number(42);
ASTNode *call_node = ast_new_call(f, &arg, 1);
/* println(42) */
```

### 6. If Statement (NODE_IF)

```c
struct {
    ASTNode *test;          /* 조건 */
    ASTNode *consequent;    /* then 블록 */
    ASTNode *alternate;     /* else 블록 (nullable) */
} if_stmt;
```

### 7. Block (NODE_BLOCK)

```c
struct {
    ASTNode **statements;
    int stmt_count;
} block;
```

### 8. Try-Catch (NODE_TRY)

```c
struct {
    ASTNode *body;              /* try 블록 */
    ASTNode *catch_clause;      /* NODE_CATCH_CLAUSE */
    ASTNode *finally_clause;    /* NODE_FINALLY_CLAUSE */
} try_stmt;
```

### 9. Member Access (NODE_MEMBER)

```c
struct {
    ASTNode *object;
    ASTNode *property;
    int is_computed;    /* 1 if obj[prop], 0 if obj.prop */
} member;
```

---

## 📚 Helper Functions

### Node 생성 함수

```c
/* Literals */
ASTNode *ast_new_number(double value);
ASTNode *ast_new_string(const char *value);
ASTNode *ast_new_bool(int value);
ASTNode *ast_new_null(void);

/* Identifiers */
ASTNode *ast_new_ident(const char *name);

/* Expressions */
ASTNode *ast_new_binary(ASTNode *left, const char *op, ASTNode *right);
ASTNode *ast_new_call(ASTNode *callee, ASTNode **args, int arg_count);
ASTNode *ast_new_member(ASTNode *object, ASTNode *property, int is_computed);

/* Statements */
ASTNode *ast_new_var_decl(const char *kind, const char *name, ASTNode *init);
ASTNode *ast_new_fn_decl(const char *name, char **params, int param_count,
                         ASTNode *body, int is_async);
ASTNode *ast_new_block(ASTNode **statements, int stmt_count);
ASTNode *ast_new_if(ASTNode *test, ASTNode *consequent, ASTNode *alternate);
ASTNode *ast_new_try(ASTNode *body, ASTNode *catch_clause, ASTNode *finally_clause);

/* Collections */
ASTNode *ast_new_array(ASTNode **elements, int element_count);
ASTNode *ast_new_object(ASTNode **properties, int property_count);
```

### Utility 함수

```c
/* Pool Management */
void ast_init_pool(size_t capacity);
void ast_free_all(void);
ASTNode *ast_alloc(NodeType type);
void ast_free(ASTNode *node);

/* Debugging */
void ast_print(ASTNode *node, int indent);
const char *ast_type_name(NodeType type);
void ast_pool_stats(void);

/* Cloning */
ASTNode *ast_clone(ASTNode *node);
```

---

## 🧪 테스트

### 테스트 파일

위치: `/home/kimjin/Desktop/kim/freelang-c/test/test_ast.c`

테스트 항목:
1. ✅ Basic Literals (숫자, 문자열, 부울, null)
2. ✅ Identifiers (변수명)
3. ✅ Binary Expressions (이항 연산자)
4. ✅ Variable Declarations (let/const/var)
5. ✅ Function Calls (함수 호출)
6. ✅ Arrays (배열 리터럴)
7. ✅ If Statements (조건문)
8. ✅ Block Statements (블록)
9. ✅ Member Access (속성 접근)
10. ✅ Try-Catch (예외 처리)
11. ✅ AST Print (디버깅 출력)
12. ✅ Pool Statistics (메모리 통계)

### 빌드 & 실행

```bash
cd /home/kimjin/Desktop/kim/freelang-c

# 전체 빌드
make all

# 테스트 실행
make test

# 크린 빌드
make clean all
```

---

## 📊 메모리 구조

### ASTNode 크기

```c
sizeof(ASTNode) =
    NodeType (4 bytes)
  + int line (4 bytes)
  + int col (4 bytes)
  + TypeAnnotation* (8 bytes)
  + union data (최대 600 bytes)
  = ~620 bytes
```

### Object Pool 메모리

```
Capacity: 10,000 nodes
Per node: ~620 bytes
Total: ~6.2 MB (초기 할당)
```

---

## 🔍 Example Usage

### 간단한 프로그램 파싱

```c
#include "ast.h"

int main(void)
{
    ast_init_pool(5000);

    /* let x = 10; */
    ASTNode *var_decl = ast_new_var_decl(
        "let",
        "x",
        ast_new_number(10)
    );

    /* println(x); */
    ASTNode *call = ast_new_call(
        ast_new_ident("println"),
        (ASTNode *[]) { ast_new_ident("x") },
        1
    );

    /* { let x = 10; println(x); } */
    ASTNode *block = ast_new_block(
        (ASTNode *[]) { var_decl, call },
        2
    );

    /* Print AST */
    ast_print(block, 0);

    /* Cleanup */
    ast_free_all();

    return 0;
}
```

---

## 🚀 다음 단계

1. **Parser 구현** (`src/parser.c`)
   - 토큰 스트림 → AST로 변환
   - 재귀 하강 파서 또는 Pratt 파서

2. **Semantic Analyzer** (`src/analyzer.c`)
   - Type 검증
   - 변수 스코프 분석
   - 함수 해결

3. **Code Generator** (`src/codegen.c`)
   - AST → Bytecode (VM용)
   - 최적화

4. **Virtual Machine** (`src/vm.c`)
   - Bytecode 실행

---

## 📝 설계 원칙

### 1. Memory Safety
- Object Pool로 메모리 관리 단순화
- 명확한 수명 관리

### 2. Performance
- Union으로 메모리 효율성
- 캐시 친화적 할당

### 3. Extensibility
- 35개 노드 타입으로 대부분의 언어 구조 표현 가능
- Generic 지원 (TypeAnnotation)

### 4. Debuggability
- `ast_print()` 함수로 트리 구조 시각화
- `ast_type_name()` 함수로 타입명 조회
- `ast_pool_stats()` 함수로 메모리 모니터링

---

## 📄 파일 목록

| 파일 | 라인 | 내용 |
|------|------|------|
| `include/ast.h` | 650 | AST 타입 정의 + 함수 선언 |
| `src/ast.c` | 650 | 구현 (할당자, 헬퍼, 디버그) |
| `test/test_ast.c` | 400 | 12개 테스트 케이스 |

**총 1,700줄**

---

## 🎯 Completion Checklist

- [x] NodeType enum (35개 타입)
- [x] ASTNode union 구조체
- [x] TypeAnnotation 구조체
- [x] Object Pool allocator
- [x] 모든 노드 타입별 구조체 정의
- [x] 35개 Helper 함수
- [x] ast_print() 디버그 함수
- [x] ast_clone() 복제 함수
- [x] 12개 테스트 케이스
- [x] 빌드 시스템 (Makefile)

**상태**: ✅ **완료**
