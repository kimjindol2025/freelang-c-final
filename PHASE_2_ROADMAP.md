# Phase 2: 완성형 FreeLang 컴파일러 개발 로드맵

## 🎯 최종 목표

**Fixed-Point Bootstrap 달성**
```
Stage 1: freelang-compiler.free (소스)
         + bin/fl (C 인터프리터)
         ↓ 컴파일
         → compiler_v1 (바이너리)

Stage 2: freelang-compiler.free (소스)
         + compiler_v1 (바이너리)
         ↓ 자신을 재컴파일
         → compiler_v2 (바이너리)

Stage 3: md5sum compiler_v1 compiler_v2
         → 동일 ✅ = 자체호스팅 완성!
```

---

## 📊 구현 단계

### **Phase 2.1: Lexer ✅ (완료)**
- ✅ 토큰 정의 (25개 토큰 타입)
- ✅ 공백 처리
- ✅ 주석 처리 (/* */ 및 //)
- ✅ 문자열 파싱 (이스케이프 포함)
- ✅ 숫자 파싱
- ✅ 식별자 및 키워드 분류
- ✅ 기호 인식 (=, ==, !=, <=, >=, 등)

**파일**: `freelang-compiler.free` (렉서 부분, ~200줄)

---

### **Phase 2.2: Parser 🔄 (진행 중)**

목표: 토큰 → AST (Abstract Syntax Tree)

**구현할 것**:
```
Program → [FunctionDef]*
FunctionDef → "fn" ID "(" [Params] ")" "{" [Statements] "}"
Statement → VarDecl | Assignment | FunctionCall | IfStmt | WhileStmt | ReturnStmt
Expression → Comparison
Comparison → Addition (("==" | "!=" | "<" | "<=" | ">" | ">=") Addition)*
Addition → Multiplication (("+" | "-") Multiplication)*
Multiplication → Primary (("*" | "/") Primary)*
Primary → NUMBER | STRING | ID | "(" Expression ")" | "[" [Expression*] "]"
```

**테스트 대상**: hello.free
```freelang
fn main() {
  println("Hello, World!")
}
main()
```

**예상 AST**:
```
Program {
  functions: [
    FunctionDef {
      name: "main",
      params: [],
      body: [
        ExprStmt {
          expr: FunctionCall {
            name: "println",
            args: [String("Hello, World!")]
          }
        }
      ]
    }
  ],
  statements: [
    ExprStmt {
      expr: FunctionCall {
        name: "main",
        args: []
      }
    }
  ]
}
```

---

### **Phase 2.3: IR Generator (계획)**

AST → Intermediate Representation (중간 표현)

**IR 구조**:
```
Instruction {
  opcode: "PUSH_CONST" | "CALL" | "RET" | ...
  operands: [...]
}
```

---

### **Phase 2.4: aarch64 Code Generator (계획)**

IR → aarch64 기계어

**예시** (hello.free 컴파일 결과):
```asm
main:
    ; 문자열 주소 로드
    adr x0, msg_0

    ; println 호출 (syscall write)
    mov x8, #64
    svc #0

    ; 반환
    ret

msg_0:
    .asciz "Hello, World!"
```

---

### **Phase 2.5: ELF Generator (계획)**

기계어 → aarch64 ELF 바이너리

**구조**:
```
ELF Header (64 bytes)
Program Header (56 bytes)
Code Section
Data Section
```

---

### **Phase 2.6: 테스트 & 고정점 검증 (최종)**

```bash
# Stage 1
./bin/fl run freelang-compiler.free hello.free -o hello_v1.elf

# Stage 2
./hello_v1.elf freelang-compiler.free hello.free -o hello_v2.elf

# Stage 3
md5sum hello_v1.elf hello_v2.elf
# → 동일하면 ✅ 성공!
```

---

## 🔧 현재 상태

| 단계 | 상태 | 파일 | 줄 수 |
|------|------|------|-------|
| **Phase 2.1: Lexer** | ✅ 작동 | freelang-compiler.free | 200+ |
| **Phase 2.2: Parser** | 🔄 진행 중 | (아래 추가) | 300+ |
| **Phase 2.3: IR Gen** | ⏳ 계획 | (아래 추가) | 200+ |
| **Phase 2.4: CodeGen** | ⏳ 계획 | (아래 추가) | 400+ |
| **Phase 2.5: ELF Gen** | ⏳ 계획 | (아래 추가) | 200+ |
| **Phase 2.6: 검증** | ⏳ 계획 | test script | 50+ |
| **합계** | | | 1350+ 줄 |

---

## 📈 예상 일정

| Phase | 예상 시간 | 난이도 |
|-------|---------|--------|
| 2.1 (Lexer) | ✅ 완료 | ⭐ 낮음 |
| 2.2 (Parser) | ~1-2시간 | ⭐⭐ 중간 |
| 2.3 (IR) | ~1시간 | ⭐⭐ 중간 |
| 2.4 (CodeGen) | ~2-3시간 | ⭐⭐⭐ 높음 |
| 2.5 (ELF) | ~1-2시간 | ⭐⭐⭐ 높음 |
| 2.6 (검증) | ~30분 | ⭐ 낮음 |
| **합계** | **~6-8시간** | |

---

## 🎯 성공 기준

✅ **최소 요구사항**:
1. hello.free 컴파일 가능
2. 생성된 바이너리 실행 가능
3. 올바른 출력 ("Hello, World!")

✅ **완전한 검증**:
1. 자체 컴파일러로 컴파일 가능
2. 고정점 도달 (md5sum 동일)
3. 진정한 자체호스팅 달성

---

## 📝 다음 단계

**Phase 2.2부터 시작**: Parser 구현
- 토큰 스트림을 받아 AST 생성
- 재귀 하강 파서 (Recursive Descent Parser) 방식 사용
- hello.free 파싱 테스트

**파일**: freelang-compiler.free에 Parser 코드 추가

---

**시작일**: 2026-03-07
**목표**: Phase 2 완성 → 고정점 달성 → 자체호스팅 완성
**진행 상황**: Phase 2.1 ✅, Phase 2.2 🔄
