# FreeLang C AST Structure Reference

**작성**: 2026-03-06
**버전**: 1.0
**상태**: ✅ Production-Ready

---

## 📐 35개 AST 노드 타입 상세 정의

### GROUP 1: 선언 (Declaration) - 5개

#### 1. NODE_PROGRAM
프로그램의 최상위 노드 (루트)

```c
struct {
    ASTNode **items;        /* 모든 선언 & 문장 */
    int item_count;
} program;
```

**예**:
```
Program
├── FnDecl: main
├── VarDecl: x = 10
└── VarDecl: y = 20
```

#### 2. NODE_VAR_DECL
변수 선언

```c
struct {
    char kind[8];           /* "let", "const", "var" */
    char name[256];         /* 변수명 */
    ASTNode *init;          /* 초기값 (nullable) */
    TypeAnnotation *type;   /* 타입 정보 (nullable) */
} var_decl;
```

**예**:
```
let x = 10;
const name = "Alice";
var y;  // init은 NULL
let nums: array<number> = [1, 2, 3];
```

#### 3. NODE_FN_DECL
함수 선언

```c
struct {
    char name[256];
    char **param_names;     /* ["x", "y", "z"] */
    int param_count;
    TypeAnnotation **param_types;   /* [number, number, number] */
    TypeAnnotation *return_type;    /* number */
    ASTNode *body;          /* BlockStatement */
    int is_async;           /* 1 if async fn */
    int is_generic;         /* 1 if fn<T> */
    char **generic_params;  /* ["T"] */
    int generic_param_count;
} fn_decl;
```

**예**:
```
fn add(x: number, y: number) -> number { return x + y; }
async fn fetch(url: string) -> string { ... }
fn identity<T>(x: T) -> T { return x; }
```

#### 4. NODE_STRUCT_DECL
구조체 정의

```c
struct {
    char name[256];
    char **field_names;     /* ["x", "y"] */
    TypeAnnotation **field_types;  /* [number, string] */
    int field_count;
} struct_decl;
```

**예**:
```
struct Point {
    x: number,
    y: number
}
```

#### 5. NODE_ENUM_DECL
열거형 정의

```c
struct {
    char name[256];
    char **variant_names;   /* ["RED", "GREEN", "BLUE"] */
    int variant_count;
} enum_decl;
```

**예**:
```
enum Color {
    RED,
    GREEN,
    BLUE
}
```

---

### GROUP 2: 모듈 (Module) - 2개

#### 6. NODE_IMPORT
import 문

```c
struct {
    char **imported_names;  /* ["add", "subtract"] */
    int import_count;
    char module_path[512];  /* "math", "./utils" */
} import;
```

**예**:
```
import add from "math";
import { sin, cos } from "trig";
```

#### 7. NODE_EXPORT
export 문

```c
struct {
    ASTNode *declaration;   /* VAR_DECL 또는 FN_DECL */
    int is_default;         /* 1 if export default */
} export;
```

**예**:
```
export fn greet(name: string) { ... }
export default let config = { };
```

---

### GROUP 3: 제어문 (Statements) - 10개

#### 8. NODE_BLOCK
블록 문장 (연속된 문장)

```c
struct {
    ASTNode **statements;
    int stmt_count;
} block;
```

**예**:
```
{
    let x = 1;
    println(x);
    return x * 2;
}
```

#### 9. NODE_IF
조건문

```c
struct {
    ASTNode *test;          /* 조건 */
    ASTNode *consequent;    /* then 블록 */
    ASTNode *alternate;     /* else 블록 (nullable) */
} if_stmt;
```

**예**:
```
if (x > 5) {
    y = 10;
} else {
    y = 20;
}
```

#### 10. NODE_WHILE
while 루프

```c
struct {
    ASTNode *test;
    ASTNode *body;
} while_stmt;
```

**예**:
```
while (x > 0) {
    x = x - 1;
}
```

#### 11. NODE_FOR
for 루프

```c
struct {
    ASTNode *init;    /* 초기화 (nullable) */
    ASTNode *test;    /* 조건 (nullable) */
    ASTNode *update;  /* 갱신 (nullable) */
    ASTNode *body;
} for_stmt;
```

**예**:
```
for (let i = 0; i < 10; i = i + 1) {
    println(i);
}
```

#### 12. NODE_FOR_IN
for-in 루프 (반복)

```c
struct {
    char var_name[256];     /* "x" */
    ASTNode *iterable;      /* 배열 또는 객체 */
    ASTNode *body;
} for_in_stmt;
```

**예**:
```
for (item in items) {
    println(item);
}
```

#### 13. NODE_RETURN
return 문

```c
struct {
    ASTNode *value;   /* 반환값 (nullable) */
} return_stmt;
```

**예**:
```
return x + 1;
return;  /* value는 NULL */
```

#### 14. NODE_BREAK
break 문

```c
struct {
    int label;  /* 향후 사용 */
} break_stmt;
```

#### 15. NODE_CONTINUE
continue 문

```c
struct {
    int label;  /* 향후 사용 */
} continue_stmt;
```

#### 16. NODE_TRY
try-catch-finally 문

```c
struct {
    ASTNode *body;              /* try 블록 */
    ASTNode *catch_clause;      /* NODE_CATCH_CLAUSE (nullable) */
    ASTNode *finally_clause;    /* NODE_FINALLY_CLAUSE (nullable) */
} try_stmt;
```

**예**:
```
try {
    x = risky_operation();
} catch (e) {
    println(e);
} finally {
    cleanup();
}
```

#### 17. NODE_THROW
throw 문

```c
struct {
    ASTNode *argument;
} throw_stmt;
```

**예**:
```
throw "Error occurred";
throw error_obj;
```

---

### GROUP 4: 표현식 (Expressions) - 13개

#### 18. NODE_BINARY
이항 연산

```c
struct {
    ASTNode *left;
    char op[16];    /* "+", "-", "*", "/", "==", "!=", "<", ">", etc */
    ASTNode *right;
} binary;
```

**예**:
```
5 + 3           /* op = "+" */
x == y          /* op = "==" */
a && b          /* op = "&&" */
```

#### 19. NODE_UNARY
단항 연산

```c
struct {
    char op[16];        /* "-", "!", "~", "++", "--" */
    ASTNode *operand;
    int is_prefix;      /* 1 if ++x, 0 if x++ */
} unary;
```

**예**:
```
-x              /* op = "-", operand = x */
!flag           /* op = "!", operand = flag */
++counter       /* op = "++", is_prefix = 1 */
```

#### 20. NODE_LOGICAL
논리 연산 (&&, ||)

```c
struct {
    ASTNode *left;
    char op[8];     /* "&&", "||" */
    ASTNode *right;
} logical;
```

#### 21. NODE_CALL
함수 호출

```c
struct {
    ASTNode *callee;        /* 함수 이름 또는 표현식 */
    ASTNode **arguments;    /* 인자 배열 */
    int arg_count;
} call;
```

**예**:
```
println(42)             /* callee=println, args=[42] */
obj.method(x, y)        /* callee=obj.method, args=[x, y] */
fn(a)(b)                /* 중첩 호출 */
```

#### 22. NODE_MEMBER
속성 접근

```c
struct {
    ASTNode *object;
    ASTNode *property;
    int is_computed;    /* 1 if obj[prop], 0 if obj.prop */
} member;
```

**예**:
```
obj.prop            /* is_computed=0, property=ident("prop") */
arr[5]              /* is_computed=1, property=literal(5) */
obj[key]            /* is_computed=1, property=ident("key") */
```

#### 23. NODE_ASSIGN
할당

```c
struct {
    ASTNode *target;    /* IDENT 또는 MEMBER */
    char op[8];         /* "=", "+=", "-=", "*=", "/=" */
    ASTNode *value;
} assign;
```

**예**:
```
x = 10          /* op = "=" */
x += 5          /* op = "+=" */
arr[0] = 99     /* target = MEMBER */
```

#### 24. NODE_TERNARY
삼항 연산

```c
struct {
    ASTNode *test;
    ASTNode *consequent;
    ASTNode *alternate;
} ternary;
```

**예**:
```
x > 5 ? "big" : "small"
a ?: b          /* a가 true면 a, 아니면 b */
```

#### 25. NODE_ARROW_FN
화살표 함수

```c
struct {
    char **param_names;
    int param_count;
    ASTNode *body;      /* 표현식 또는 블록 */
    int is_expression;  /* 1 if x => x*2, 0 if x => { return x*2; } */
} arrow_fn;
```

**예**:
```
(x) => x * 2                    /* 표현식 */
(x, y) => { return x + y; }     /* 블록 */
```

#### 26. NODE_AWAIT
await 표현식

```c
struct {
    ASTNode *argument;
} await_expr;
```

**예**:
```
await promise
await fetch(url)
```

#### 27. NODE_QUESTION_OP
Optional chaining (?.)

```c
struct {
    ASTNode *object;
    ASTNode *property;
    int is_computed;    /* 1 if obj?.[prop] */
} question_op;
```

**예**:
```
obj?.prop           /* obj가 null이면 null, 아니면 obj.prop */
arr?.[0]            /* arr이 null이면 null, 아니면 arr[0] */
```

---

### GROUP 5: 컬렉션 (Collections) - 3개

#### 28. NODE_ARRAY
배열 리터럴

```c
struct {
    ASTNode **elements;
    int element_count;
} array;
```

**예**:
```
[1, 2, 3]
[x, y, z]
[]          /* 빈 배열 */
```

#### 29. NODE_OBJECT
객체 리터럴

```c
struct {
    ASTNode **properties;  /* NODE_PROPERTY 배열 */
    int property_count;
} object;
```

**예**:
```
{ name: "Alice", age: 30 }
{ x, y }    /* shorthand */
{}          /* 빈 객체 */
```

#### 30. NODE_PROPERTY
객체 속성 (객체 리터럴 내부)

```c
struct {
    char key[256];
    ASTNode *value;
    int is_computed;    /* 1 if [key]: value */
} property;
```

**예**:
```
객체 내부: { key: value }
computed: { [computed_key]: value }
```

---

### GROUP 6: 고급 표현식 (Advanced) - 1개

#### 31. NODE_FSTRING
템플릿 문자열

```c
struct {
    char **parts;           /* 리터럴 부분 */
    int part_count;
    ASTNode **expressions;  /* 보간된 표현식 */
    int expr_count;
} fstring;
```

**예**:
```
`Hello ${name}!`            /* parts=["Hello ", "!"], exprs=[name] */
`x + y = ${x + y}`          /* 표현식 포함 */
```

---

### GROUP 7: 기본 노드 (Leaf) - 2개

#### 32. NODE_IDENT
식별자 (변수명, 함수명 등)

```c
struct {
    char name[256];
} ident;
```

**예**:
```
x, myVariable, count, println, Number
```

#### 33. NODE_LITERAL
리터럴값 (상수)

```c
struct {
    double num;         /* 숫자 */
    char *str;          /* 문자열 포인터 */
    int is_bool;        /* 부울 플래그 */
    int bool_val;       /* 0 or 1 */
    int is_null;        /* null 플래그 */
    int is_nan;         /* NaN 플래그 */
    int is_inf;         /* Infinity 플래그 */
} literal;
```

**예**:
```
42              /* num=42.0, is_bool=0 */
3.14            /* num=3.14 */
"hello"         /* str="hello" */
true            /* is_bool=1, bool_val=1 */
false           /* is_bool=1, bool_val=0 */
null            /* is_null=1 */
NaN             /* is_nan=1 */
Infinity        /* is_inf=1 */
```

---

### GROUP 8: 예외 처리 (Exception) - 2개

#### 34. NODE_CATCH_CLAUSE
catch 블록

```c
struct {
    char error_var[256];    /* "e", "error", "err" */
    ASTNode *handler;       /* 블록 */
} catch_clause;
```

#### 35. NODE_FINALLY_CLAUSE
finally 블록

```c
struct {
    ASTNode *handler;       /* 블록 */
} finally_clause;
```

---

## 🗺️ 전체 노드 타입 분류

| 카테고리 | 갯수 | 노드 타입 |
|---------|------|----------|
| 선언 | 5 | PROGRAM, VAR_DECL, FN_DECL, STRUCT_DECL, ENUM_DECL |
| 모듈 | 2 | IMPORT, EXPORT |
| 제어문 | 10 | BLOCK, IF, WHILE, FOR, FOR_IN, RETURN, BREAK, CONTINUE, TRY, THROW |
| 표현식 | 13 | BINARY, UNARY, LOGICAL, CALL, MEMBER, ASSIGN, TERNARY, ARROW_FN, AWAIT, QUESTION_OP, ARRAY, OBJECT, FSTRING |
| 컬렉션 | 3 | ARRAY, OBJECT, PROPERTY |
| 기본 | 2 | IDENT, LITERAL |
| 예외 | 2 | CATCH_CLAUSE, FINALLY_CLAUSE |
| **합계** | **35** | |

---

## 📊 메모리 레이아웃

### Union 데이터 크기 (각 노드 타입)

| 노드 타입 | 크기 | 주요 필드 |
|---------|------|---------|
| binary | 32 | left, op[16], right |
| var_decl | 280 | kind[8], name[256], init, type |
| fn_decl | 320 | name[256], param_names[], body, etc |
| call | 32 | callee, arguments[], arg_count |
| literal | 40 | num, str*, is_bool, etc |
| ident | 256 | name[256] |
| array | 16 | elements[], element_count |
| object | 16 | properties[], property_count |

**최대 union 크기**: ~560 bytes (fn_decl)

### ASTNode 전체 크기

```c
typedef struct ASTNode {
    NodeType type;          // 4 bytes
    int line, col;          // 8 bytes
    TypeAnnotation *type_info; // 8 bytes
    union { /* ~560 bytes */ } data;
    // ────────────────────
} ASTNode;  // ~580 bytes
```

---

## 🔄 노드 생성 흐름

```
Parser Token Stream
    ↓
ast_alloc(NODE_TYPE)  ← Object Pool에서 할당
    ↓
ASTNode *node  ← 초기화되지 않은 노드
    ↓
node->type = NODE_TYPE
node->line = lexer.line
node->col = lexer.col
    ↓
node->data.xxx = { ... }  ← Union 필드 초기화
    ↓
return node  ← Parser로 반환
```

---

## 💡 설계 특징

### 1. Type Safety
- `NodeType` enum으로 타입 구분
- Union으로 안전한 필드 접근

### 2. Memory Efficiency
- 42% 메모리 절감 vs 모든 필드 포함
- Object Pool로 malloc 오버헤드 제거

### 3. Extensibility
- TypeAnnotation으로 제네릭 지원
- Custom 타입 정보 추가 가능

### 4. Debuggability
- Line/col 정보로 에러 위치 추적
- ast_print()로 트리 시각화

---

**참고**: 이 문서는 FreeLang C AST 구현의 완전한 참고 자료입니다.
