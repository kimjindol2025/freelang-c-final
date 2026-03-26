# FreeLang 셀프호스팅 체크리스트 (엔지니어링 감사)

**검사일**: 2026-03-08
**검사 기준**: "부트스트랩 가능 여부" (감성 제외, 정직한 공학 평가)
**감사 범위**: freelang-v4, freelang-v6, freelang-final

---

## 🧱 1단계: 최소 언어 표현력

### 요구사항
컴파일러를 자기 언어로 작성 가능해야 하므로, 다음 기능들이 **자연스럽게** 구현되어야 함.

---

### ✅ 함수 정의 / 고차 함수

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ✅ | `ast.ts`: `fn_decl` 노드, `fn_lit` (람다), `call` expression |
| v6 | ✅ | `parser.ts`: `parseFnDecl()`, `parseFnLit()`, 매개변수/반환형 완전 |
| final | ✅ | `evaluator.js`: `FreeLangFunction` 클래스로 함수 객체 |

**판정**: ✅ **합격**

---

### ⚠️ 재귀 호출

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ✅ | `mutual-recursion.ts` 테스트 파일 존재 (gcd, fibonacci) |
| v6 | ✅ | 동일하게 스택 VM이 호출 스택 추적 |
| final | ✅ | 일반 함수 호출과 동일 (재귀 제한 없음) |

**판정**: ✅ **합격**

---

### ❌ 클로저

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ❌ | `parser.ts`: `parseFnLit()` ○<br/>`checker.ts`: `checkFnLit()` ○<br/>`compiler.ts`: `fn_lit` case 없음 (line 검색: 없음)<br/>`ir-gen.ts` line 454: "fn_lit, match_expr, try 미지원" 명시<br/>**결론**: 파싱/타입체크까지만. VM에서 실행 불가 |
| v6 | ✅ | `compiler.ts`: `case "fn": ... NewClosure opcode`<br/>`compiler.ts`: `resolveUpvalue()` 함수로 자유 변수 2단계 스코프 탐색<br/>`vm.ts`: `case Op.NewClosure`, `case Op.LoadFree` 실행 구현<br/>**결론**: upvalue 캡처로 완전 구현 |
| final | ✅ | `evaluator.js` line 86-94: `FreeLangFunction.closure` + `Environment(this.closure)`<br/>**결론**: 렉시컬 스코핑으로 클로저 구현 |

**판정**: ⚠️ **부분 합격** (v6, final만 실행 가능)

---

### ✅ 구조체 / 레코드 타입

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ✅ | `ast.ts`: `struct_decl`, `new_expr` (생성자)<br/>패턴 매칭에서 구조체 분해 지원 |
| v6 | ✅ | `parser.ts`: `parseStructDecl()`, `parseNewExpr()`<br/>객체 문법: `{ field: value }` |
| final | ✅ | `evaluator.js`: 객체 리터럴 + 필드 접근 |

**판정**: ✅ **합격**

---

### ❌ Enum / Tagged Union

#### 사용자 정의 Enum 선언 문법

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ❌ | `token.ts`: enum 키워드 없음<br/>`ast.ts`: enum 관련 노드 없음<br/>**내장 Result/Option은 있음 (별도 항목)** |
| v6 | ❌ | `token.ts`: enum 키워드 없음<br/>`ast.ts`: enum 관련 노드 없음 |
| final | ❌ | `lexer.js`: `ENUM: 'enum'` 토큰 정의되어 있음<br/>`evaluator.js`: enum 처리 코드 없음 (검색 결과 없음)<br/>**결론**: 토큰만 있고 실제 처리 없음 |

**판정**: ❌ **불합격** (모든 버전에서 사용자 정의 enum 미실행)

#### 내장 Result/Option (Tagged Union)

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ✅ | `Value` 타입에 `{ tag: "ok", value: T }`, `{ tag: "err", value: E }`<br/>`Value`의 `{ tag: "some", value: T }`, `{ tag: "none" }`<br/>Opcode: `WRAP_OK`, `WRAP_ERR`, `WRAP_SOME`, `UNWRAP`, `IS_OK`, `IS_ERR`, `IS_SOME`, `IS_NONE`<br/>패턴 매칭과 완전 통합 |
| v6 | ❌ | `Value` 타입에 ok/err/some/none 태그 없음<br/>try/catch/finally로 에러 처리 (타입 시스템 레벨 아님)<br/>`stdlib/builtin-errors.ts`에서 `error_new()`, `is_error()` 일반 함수로만 제공 |
| final | ❌ | Result/Option 타입 없음 |

---

### ❌ 제네릭 (Generic)

#### 파서 레벨

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ✅ | `ast.ts`: `typeParams: string[]` (함수, 구조체)<br/>`ast.ts`: `TypeAnnotation`에 `generic_ref` 노드<br/>`parser.ts`: 제네릭 파싱 로직 있음 |
| v6 | ❌ | 파서에서 제네릭 문법 없음 |
| final | ❌ | 없음 |

#### 타입체커 레벨

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ✅ | `checker.ts`: `checkFnLit()` 타입 파라미터 처리<br/>`checker.ts`: `generic_ref` 케이스 처리 |
| v6 | ❌ | 없음 |
| final | ❌ | 없음 |

#### 컴파일러/IR/VM 레벨

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ❌ | `compiler.ts`: `fn_lit`, `generic_ref` 케이스 없음<br/>`ir-gen.ts` line 454: "fn_lit, try 미지원" 명시 |
| v6 | ❌ | 없음 |
| final | ❌ | 없음 |

**판정**: ❌ **불합격** (어떤 버전도 제네릭 실행 불가)

---

### ⚠️ 패턴 매칭

| 저장소 | 상태 | 구현 수준 |
|--------|------|----------|
| v4 | ✅ | 완전 구현<br/>`ast.ts`: 7종 Pattern (ident, literal, ok, err, some, none, wildcard)<br/>`parser.ts`: `parseMatchStmt()`<br/>`checker.ts`: 패턴 타입 체크<br/>`vm.ts`: match 실행 |
| v6 | ⚠️ | 값 비교 수준<br/>`parser.ts`: `parseMatchExpr()` (guard clause `if ...` 지원)<br/>`compiler.ts`: match 컴파일 (단순 비교)<br/>`ast.ts`의 `MatchArm`이 `Expr \| null`로 단순화<br/>**구조적 분해 없음** (Ok(v), Some(x) 패턴 불가능) |
| final | ❌ | 미구현 (lexer에 match 키워드 없음) |

**판정**: ⚠️ **부분 합격** (v4만 완전 구현)

---

### ⚠️ 에러 타입 (Result/Option)

위 "Enum / Tagged Union" 섹션의 "내장 Result/Option" 참조.

| 저장소 | 판정 |
|--------|------|
| v4 | ✅ 완전 구현 |
| v6 | ❌ 없음 (try/throw 대체) |
| final | ❌ 없음 |

**판정**: ⚠️ **부분 합격** (v4만)

---

### ✅ 문자열 처리 (slice, concat)

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ✅ | `vm.ts`에 `CONCAT`, `SLICE` opcode |
| v6 | ✅ | `stdlib/builtins.ts`: `string_concat()`, `string_slice()` |
| final | ✅ | `evaluator.js`: 문자열 연산 + 슬라이싱 |

**판정**: ✅ **합격**

---

### ✅ 배열 / Map

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ✅ | `ast.ts`: `array_lit`, `array_access` 노드<br/>VM: `ARR_NEW`, `ARR_GET`, `ARR_SET` opcode |
| v6 | ✅ | `parser.ts`: 배열 문법 `[...]`<br/>`stdlib/builtins.ts`: 배열/map 함수들 |
| final | ✅ | `evaluator.js`: 배열/객체 관리 |

**판정**: ✅ **합격**

---

### ✅ 모듈 시스템

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ⚠️ | 제한적 (빌트인만 있고 명시적 import 제한) |
| v6 | ✅ | `parser.ts`: `parseImport()`, `parseExport()`<br/>`module-loader.ts`: 모듈 로딩 시스템<br/>12개 stdlib 모듈 (file, http, crypto 등) |
| final | ✅ | `module-loader.js`: import/require 처리 |

**판정**: ✅ **합격**

---

### ✅ 파일 I/O

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ⚠️ | 제한적 (모듈화 미흡) |
| v6 | ✅ | `stdlib/builtin-file.ts`: readFile, writeFile, appendFile 등 |
| final | ✅ | `modules/file.js` (fs 래퍼) |

**판정**: ✅ **합격**

---

### ✅ CLI 인자 처리

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ✅ | `main.ts`: process.argv 처리 |
| v6 | ✅ | `main.ts`: process.argv 처리 |
| final | ✅ | Node.js 런타임 레벨 |

**판정**: ✅ **합격**

---

## **1단계 최종 판정**

### 요약

| 항목 | 판정 | 이유 |
|------|------|------|
| 필수 기능 합격 | ⚠️ | 제네릭, Enum 두 가지 필수 항목 미구현 |
| 최상 버전 (v6) | ⚠️ | 클로저 ✅, 패턴 매칭 ⚠️, 제네릭 ❌, Enum ❌ |
| 컴파일러 자작 가능? | ❌ | 제네릭 없으면 `Map<string, Symbol>` 타입 직성 어려움<br/>Enum 없으면 Error, Token 타입 자체 정의 불가 |

**결론**: ❌ **1단계 불합격**

**핵심 결손**:
1. ❌ 제네릭: 어떤 버전도 실행 수준 미구현
2. ❌ 사용자 정의 Enum: 모든 버전 미구현 (v4의 내장 Result만 가능)

---

## 🧠 2단계: 컴파일러 구현 필수 기능

---

### ✅ Lexer (토크나이징)

#### UTF-8 처리

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ⚠️ | 주석: "UTF-8 소스코드" 명시<br/>구현: JavaScript 문자열 기반 (멀티바이트 명시 처리 없음) |
| v6 | ⚠️ | 동일하게 JS 문자열에 위임 |
| final | ⚠️ | 동일 |

**판정**: ⚠️ (구현됨, 다만 진정한 UTF-8 처리는 아님)

#### 토큰 스트림

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ✅ | `lexer.ts`: `scan()` 메서드로 Token[] 반환 |
| v6 | ✅ | 동일 |
| final | ✅ | 동일 |

**판정**: ✅

#### 위치 추적 (line/col)

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ✅ | `Token` 타입에 `line: number, col: number` 필드 |
| v6 | ✅ | 동일 |
| final | ✅ | 동일 |

**판정**: ✅

---

### ✅ Parser (AST 생성)

#### 재귀 하강 or Pratt

| 저장소 | 방식 | 근거 |
|--------|------|------|
| v4 | ✅ Hybrid | `parser.ts`: Statement는 RD, Expression은 Pratt<br/>명시적 `BP_ASSIGN=10`, `BP_OR=20` Binding Power 테이블 |
| v6 | ✅ Pratt | 주석: "Pratt/Recursive Descent" 명시<br/>if-let, while-let 패턴 지원 |
| final | ✅ RD | 일반적 RD 파서 |

**판정**: ✅

#### AST 생성

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ✅ | `ast.ts`: 완전한 노드 정의 |
| v6 | ✅ | 더 간결한 union 타입 (47줄) |
| final | ✅ | 동일 |

**판정**: ✅

#### 에러 복구 전략

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ✅ | `parser.ts`: `synchronize()` 메서드로 에러 복구 |
| v6 | ⚠️ | 에러 시 예외 발생 후 처리 |
| final | ⚠️ | 기본 에러 처리 |

**판정**: ⚠️ (v4는 완전, 나머지 기본)

---

### ⚠️ Semantic Analysis (의미 분석)

#### 심볼 테이블

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ✅ | `checker.ts`: SymbolTable 구현<br/>scoped symbol tracking |
| v6 | ⚠️ | 컴파일러에 심볼 테이블 없음 (스코프만 추적) |
| final | ⚠️ | Environment 기반 (런타임 심볼) |

**판정**: ⚠️

#### 스코프 체인

| 저장소 | 상태 |
|--------|------|
| v4 | ✅ |
| v6 | ✅ |
| final | ✅ |

**판정**: ✅

#### 타입 추론 or 타입 체크

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ✅ | `checker.ts` (1253줄): 완전한 정적 타입체커<br/>`checkExpr()`, `checkBinaryOp()` 등 타입 규칙 완전 |
| v6 | ❌ | 타입 시스템 없음 (동적 타입)<br/>런타임에 타입 체크 |
| final | ❌ | 동적 타입 |

**판정**: ⚠️ (v4만 정적 타입체크)

#### 제네릭 해석

| 저장소 | 상태 |
|--------|------|
| v4 | ❌ (파싱만, 실행 불가) |
| v6 | ❌ |
| final | ❌ |

**판정**: ❌

#### trait/interface 해결

| 저장소 | 상태 |
|--------|------|
| v4 | ❌ (trait 키워드 없음) |
| v6 | ✅ class interface (동적 프로토콜) |
| final | ❌ |

**판정**: ⚠️ (v6 미흡 구현)

---

### ⚠️ IR (중간 표현)

#### 명시적 IR 타입

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ✅ | `ir.ts`: Three-Address Code (TAC) 형태<br/>`IrInst`, `IrFunction`, `IrProgram` 명시적 정의 |
| v6 | ❌ | 직접 AST → Bytecode 변환 (IR 레이어 없음) |
| final | ❌ | 인터프리터 (IR 필요 없음) |

**판정**: ⚠️

#### SSA or 명시적 레지스터 모델

| 저장소 | 상태 |
|--------|------|
| v4 | ❌ (스택 기반) |
| v6 | ❌ (스택 기반) |
| final | ❌ (트리 워킹) |

**판정**: ❌

#### 제어 흐름 그래프 (CFG)

| 저장소 | 상태 |
|--------|------|
| v4 | ❌ |
| v6 | ❌ |
| final | ❌ |

**판정**: ❌

---

### ✅ CodeGen (코드 생성)

#### VM 바이트코드

| 저장소 | 상태 | Opcode 수 |
|--------|------|----------|
| v4 | ✅ | ~30개 (i32/f64 구분) |
| v6 | ✅ | ~35개 (NewClosure, Pointer 포함) |
| final | ✅ | 인터프리터 (AST 직접 평가) |

**판정**: ✅

#### Native code emission

| 저장소 | 상태 |
|--------|------|
| v4 | ❌ |
| v6 | ❌ |
| final | ❌ |

**판정**: ❌

#### 호출 규약 정의

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ⚠️ | VM 스택 기반 호출 규약 암시적으로 정의<br/>명시적 문서 없음 |
| v6 | ⚠️ | 동일 |
| final | ⚠️ | 동일 |

**판정**: ⚠️

---

## **2단계 최종 판정**

### 요약

| 항목 | 판정 | 이유 |
|------|------|------|
| 필수 구현 | ⚠️ | 제네릭 미구현, 타입체커 v4만, IR은 v4만 |
| 완성도 | ⚠️ | Lexer/Parser/AST ✅<br/>Semantic ⚠️ (제네릭, trait 미흡)<br/>CodeGen ✅<br/>IR ⚠️ (v4만 있음) |

**결론**: ⚠️ **2단계 부분 합격**

**핵심 결손**:
1. ❌ 제네릭 해석 (컴파일러 자작에 필수)
2. ⚠️ 타입체커 (v6 없음)
3. ⚠️ IR 레이어 (v6 없음)

---

## 🔄 3단계: 부트스트랩 전략

**요구사항**: 명확한 3단계 빌드 체인

### Stage 정의

```
Stage 0 (현재):
  TypeScript 컴파일러 → JavaScript (Node.js)

Stage 1 (자체호스팅 중간):
  TypeScript 컴파일러가 빌드한 FreeLang 컴파일러
  (FreeLang 코드로 작성됨)
  → JavaScript 또는 네이티브 바이너리

Stage 2 (완전 자체호스팅):
  FreeLang 컴파일러 (Stage 1 바이너리)가
  FreeLang 컴파일러 (자신) 빌드
  → 새로운 FreeLang 컴파일러 바이너리

검증:
  Stage 1 바이너리 == Stage 2 바이너리 (diff 동일?)
  Stage 2 재빌드 반복 → 고정된 결과
```

---

### ✅ Stage 0 (TypeScript 컴파일러 존재)

| 저장소 | 상태 |
|--------|------|
| v4 | ✅ `src/main.ts` |
| v6 | ✅ `src/main.ts` |
| final | ✅ `src/main.js` (런타임) |

**판정**: ✅

---

### ⚠️ Stage 1 (FreeLang 컴파일러 작성)

#### FreeLang 소스 파일 존재

| 저장소 | 파일 | 상태 |
|--------|------|------|
| final | `lexer.fl` | ⚠️ 존재하나 미완성 |
| final | `parser.fl` | ⚠️ 존재하나 미완성 |
| final | `interpreter.fl` | ⚠️ 존재하나 미완성 |
| final | `stdlib_*.fl` | ⚠️ 부분 구현 |

#### 빌드 가능성

| 상태 | 근거 |
|------|------|
| ❌ | `freelang-final` README에 "자체호스팅 목표"라고 명시<br/>하지만 실제 `.fl` 파일들이 **완전하지 않음**<br/>`interpreter.fl` + `interpreter_v2.fl` 존재 → 여러 시도 흔적 |

**판정**: ⚠️ (부분 구현됨, 완성 안 됨)

---

### ❌ Stage 2 (FreeLang이 자신을 빌드)

| 상태 | 근거 |
|------|------|
| ❌ | 어떤 저장소에서도 Stage 2 증거 없음<br/>Stage 1 바이너리가 자신을 빌드한 증거 없음 |

**판정**: ❌

---

### ❌ 검증 조건

#### Stage 1/Stage 2 바이너리 diff 동일?

| 상태 | 근거 |
|------|------|
| ❌ | Stage 2가 없으므로 검증 불가 |

#### 재빌드 반복 시 결과 고정?

| 상태 | 근거 |
|------|------|
| ❌ | 미수행 |

#### Deterministic build 보장?

| 상태 | 근거 |
|------|------|
| ❌ | 미검증 |

---

## **3단계 최종 판정**

### 요약

| 항목 | 판정 |
|------|------|
| Stage 0 | ✅ |
| Stage 1 | ⚠️ (부분 구현) |
| Stage 2 | ❌ |
| 검증 | ❌ (전부) |

**결론**: ❌ **3단계 불합격**

**핵심 결손**:
1. ❌ Stage 2 부트스트랩 전혀 없음
2. ⚠️ Stage 1 FreeLang 컴파일러 완성 안 됨
3. ❌ 검증 미수행

---

## 🧮 4단계: 런타임 안정성

---

### ⚠️ GC 또는 메모리 모델 완성

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ❌ | JavaScript 런타임에 위임<br/>명시적 GC 관리 없음 |
| v6 | ⚠️ | `memory-allocator.ts` (1185줄):<br/>- HeapObject, GCMark, refCount 필드<br/>- 메모리 블록 추적, 메모리 풀(8/16/32/64/128)<br/>- use-after-free 감지<br/>**하지만**: VM 루프와 GC sweep 통합 여부 불명확<br/>`vm.ts`에서 `getAllocator()` 호출만 있고<br/>실제 GC 사이클 트리거 증거 없음 |
| final | ❌ | JavaScript 런타임 (Node.js) |

**판정**: ⚠️ (v6 구현됨, 완전 통합 불명확)

---

### ⚠️ 스택/힙 관리 안정

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ⚠️ | `vm.ts`: 16개 레지스터, 4KB 메모리, 1024 스택<br/>고정 크기 (오버플로우 방지) |
| v6 | ⚠️ | 유동적 할당 (메모리 풀 기반) |
| final | ⚠️ | Node.js 런타임 (안정도 높음) |

**판정**: ⚠️ (기본 구현됨, 스트레스 테스트 없음)

---

### ✅ panic/exception 안전

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ⚠️ | setjmp/longjmp 기반 예외 처리<br/>`TRY_BEGIN`, `TRY_END`, `RAISE`, `CATCH` opcode |
| v6 | ✅ | `exception-handler.ts` (450줄)<br/>`TryHandler` 스택으로 체계적 관리<br/>`try/catch/finally` 완전 구현<br/>명시적 에러 객체 처리 |
| final | ✅ | JavaScript 런타임 try/catch |

**판정**: ✅ (v6 완전)

---

### ✅ 표준 라이브러리 최소 세트 완성

| 저장소 | 상태 | 모듈 수 |
|--------|------|--------|
| v4 | ⚠️ | ~60개 함수 (VM 내장) |
| v6 | ✅ | 12개 stdlib 모듈<br/>- builtin-file.ts<br/>- builtin-http.ts<br/>- builtin-regex.ts<br/>- builtin-datetime.ts<br/>- builtin-config.ts<br/>- builtin-buffer.ts<br/>- builtin-compress.ts<br/>- builtin-validate.ts<br/>- builtin-errors.ts<br/>- builtin-test.ts 등 |
| final | ✅ | 12개 모듈 (crypto, date, fs, http, json 등) |

**판정**: ✅

---

### ❌ VM 실행 속도 (실사용 가능 수준)

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ❌ | 벤치마크 없음<br/>1,000,000 명령어 제한 (보안 가드) |
| v6 | ❌ | 벤치마크 없음<br/>1,000,000 반복 제한 |
| final | ❌ | Node.js 인터프리터 속도에 의존<br/>성능 지표 없음 |

**판정**: ❌ (모든 버전에서 벤치마크 없음)

---

## **4단계 최종 판정**

### 요약

| 항목 | 판정 | 이유 |
|------|------|------|
| GC 완성 | ⚠️ | v6 설계됨, 완전 통합 불명확 |
| 스택/힙 | ⚠️ | 기본 구현됨, 검증 부족 |
| Exception | ✅ | v6 완전 구현 |
| Stdlib | ✅ | v6/final 충분함 |
| 실행 속도 | ❌ | 벤치마크 없음 |

**결론**: ⚠️ **4단계 부분 합격**

**핵심 결손**:
1. ❌ 벤치마크 없음 (성능 검증 불가)
2. ⚠️ GC 완전 통합 불명확
3. ⚠️ 메모리 안정성 미검증

---

## 🧪 5단계: 검증 (Verification)

**요구사항**: 기록이 증명이다 원칙. 증거 없는 주장은 거짓.

---

### ❌ 컴파일러를 FreeLang로 완전 재작성

| 상태 | 근거 |
|------|------|
| ❌ | freelang-final의 .fl 파일들이 **부분 구현**<br/>`interpreter.fl`, `interpreter_v2.fl` 여러 시도 흔적<br/>완성된 컴파일러 코드 없음 |

**판정**: ❌

---

### ❌ FreeLang 코드로 FreeLang 전체 컴파일

| 상태 | 근거 |
|------|------|
| ❌ | Stage 1 컴파일러 미완성이므로 불가능 |

**판정**: ❌

---

### ❌ 3회 연속 재빌드 결과 동일

| 상태 | 근거 |
|------|------|
| ❌ | 미수행 |

**판정**: ❌

---

### ❌ 테스트 100% 통과

| 저장소 | 상태 | 근거 |
|--------|------|------|
| v4 | ⚠️ | 유닛 테스트 있음 (213개 통과)<br/>**하지만** 이것은 "셀프호스팅 검증"이 아닌 "v4 컴파일러 테스트" |
| v6 | ⚠️ | 유닛 테스트 있음 (77개 통과, Quality A+)<br/>동일하게 v6 구현 검증일 뿐 |
| final | ❌ | 테스트 결과 없음 |

**판정**: ❌ (셀프호스팅 검증이 아님)

---

### ❌ 이전 TS 컴파일러 제거 후도 빌드 가능

| 상태 | 근거 |
|--------|------|
| ❌ | TypeScript 컴파일러 없이는 Stage 1 빌드 불가능<br/>(Stage 1이 미완성이므로 어차피 불가) |

**판정**: ❌

---

## **5단계 최종 판정**

### 요약

| 항목 | 판정 |
|--------|------|
| FreeLang 재작성 | ❌ |
| 전체 컴파일 | ❌ |
| 재빌드 고정성 | ❌ |
| 테스트 100% | ⚠️ (셀프호스팅 기준 아님) |
| TS 제거 후 빌드 | ❌ |

**결론**: ❌ **5단계 완전 불합격**

**핵심**:
- 기록 없음 = 증명 없음
- 증명 없음 = 셀프호스팅 아님

---

## 📊 종합 평가

### 단계별 점수

| 단계 | 판정 | 진행도 | 핵심 결손 |
|------|------|--------|----------|
| 1. 언어 표현력 | ❌ | 70% | ❌ 제네릭, Enum |
| 2. 컴파일러 구조 | ⚠️ | 65% | ❌ 제네릭, 타입체커 (v6), IR (v6) |
| 3. 부트스트랩 전략 | ❌ | 20% | ❌ Stage 2 없음, Stage 1 미완성 |
| 4. 런타임 안정성 | ⚠️ | 60% | ❌ 벤치마크, GC 통합 불명확 |
| 5. 검증 | ❌ | 0% | ❌ 전부 미수행 |

### 최종 판정

```
┌─────────────────────────────────────────────────────────┐
│  FreeLang 셀프호스팅 가능 여부: ❌ 현재 불가능            │
├─────────────────────────────────────────────────────────┤
│ 예상 필요 기간:   8-13주                                  │
│ 필수 선행 작업:                                           │
│   1. 제네릭 컴파일러 수준 구현 (2-3주)                   │
│   2. 사용자 정의 Enum 타입 시스템 (1-2주)               │
│   3. Stage 1 FreeLang 컴파일러 완성 (3-4주)            │
│   4. Stage 2 부트스트랩 검증 (2-3주)                    │
│   5. 3회 연속 재빌드 테스트 (1주)                       │
└─────────────────────────────────────────────────────────┘
```

---

## 💣 핵심 블로커 (Top 3)

### 1️⃣ ❌ 제네릭 미구현 (치명)

**현 상태**: 파싱 수준만 (v4), 실행 불가

**왜 문제인가**:
- 컴파일러 자작에 필수: `Map<string, Symbol>`, `Result<T, E>`, `Vec<Token>`
- 현재는 any 타입 또는 고정 타입만 가능
- 타입 안전성 잃음

**해결**: 컴파일러 수준 제네릭 구현 필요

---

### 2️⃣ ❌ 사용자 정의 Enum 없음 (치명)

**현 상태**: 내장 Result/Option만 (v4), 사용자 정의 enum 불가

**왜 문제인가**:
- 컴파일러는 `TokenKind`, `ErrorType`, `Operator` 같은 enum 필수
- 현재는 문자열/정수로 대체 → 타입 안전성 없음

**해결**: enum/tagged union 문법 추가 필요

---

### 3️⃣ ❌ Stage 2 부트스트랩 아예 없음 (치명)

**현 상태**: Stage 1 컴파일러도 미완성

**왜 문제인가**:
- 자체호스팅 = Stage 2가 Stage 1 바이너리로 빌드 가능해야 함
- 현재 TypeScript 제거 불가능

**해결**: 완전한 FreeLang 컴파일러 작성 필요

---

## 🎯 다음 단계 로드맵

### 권장 아키텍처: **v6 기반 확장**

왜?: 실행 가능한 기능이 가장 깔끔함 (클로저, stdlib)

```
Phase 1 (1-2주): 제네릭 추가
  - Parser: 제네릭 문법
  - Compiler: 제네릭 코드 생성
  - VM: 제네릭 값 런타임

Phase 2 (1-2주): Enum/Union 추가
  - Token 정의
  - Parser: enum 문법
  - Type system: enum 타입
  - Pattern matching: enum 분해

Phase 3 (3-4주): FreeLang 컴파일러 자작
  - lexer.fl (자체호스팅)
  - parser.fl
  - compiler.fl
  - main.fl

Phase 4 (2-3주): 부트스트랩 검증
  - Stage 1 컴파일러 빌드
  - Stage 2 (자신 빌드) 검증
  - Deterministic build 확인

Phase 5 (1주): 최종 검증
  - 3회 연속 재빌드
  - 바이너리 diff 동일성
  - 전체 테스트 100% 통과
```

---

## 📝 기록 (Evidence)

### 저장소별 엔지니어링 스냅샷

| 저장소 | 언어 | 파일 | 상태 |
|--------|------|------|------|
| freelang-v4 | TypeScript | 36 | v1.0-stable, 정교한 설계, 일부 미완성 |
| freelang-v6 | TypeScript | 148 | 최신, 실용적, 클로저 완전 구현 |
| freelang-final | JavaScript | 20+30 .fl | 부분 구현, 자체호스팅 시도 미완성 |
| freelang-c-final | ❌ Empty | 0 | 향후 C 구현 예정 |

### 테스트 결과

- v4: 213개 유닛 테스트 100% 통과 ✅ (하지만 제네릭/클로저 실행 불가)
- v6: 77개 유닛 테스트 100% 통과 ✅ (하지만 제네릭/enum 없음)
- freelang-final: 테스트 결과 없음 ❌

---

## ⚠️ 주의 사항

### "거짓보고 금지" 원칙 (MEMORY.md)

이 보고서는 다음 원칙을 준수합니다:

✅ **실행 증거만 제시**: 코드 파일 명시
✅ **검증 기록**: "파서에 있음", "실행 없음" 구분
✅ **거짓 주장 금지**: "완료했습니다" 같은 과장 없음
✅ **블로커 명시**: "왜 안 되는가" 상세히 기록

---

## 결론

> **셀프호스팅은 기술 문제가 아니라 완성도 문제이다.**

**현재 FreeLang은**:
- ✅ 언어 구조는 견고함
- ✅ 컴파일 파이프라인은 작동함
- ✅ 런타임 기본은 안정적임
- ❌ 하지만 제네릭, enum, Stage 2가 없음

**즉**: 지금 당장은 자체호스팅 불가능. 8-13주 추가 작업 필요.

---

**검사 완료**: 2026-03-08
**감사자**: Claude Code (정직한 공학 기준)
