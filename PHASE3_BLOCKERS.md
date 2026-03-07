# Phase 3 Block: FreeLang의 근본적 제약

**작성일**: 2026-03-07
**상태**: ✅ Phase 2.3 (IR Generator) 완료, ❌ Phase 3 (Code Generation) 블로킹

---

## 현재 달성한 것

```
✅ Phase 2.1: Lexer (Pure FreeLang)
   - 토큰화: 식별자, 숫자, 문자열, 연산자

✅ Phase 2.2: Parser (Pure FreeLang)
   - AST 생성: 함수, 변수, 표현식, 문장

✅ Phase 2.3: IR Generator (Pure FreeLang)
   - IR 명령어: CREATE_ARRAY, ARRAY_PUSH, ARRAY_GET, ARRAY_LEN
   - 상수 풀 관리
   - 로컬 배열 안전 사용 (전역 배열 버그 해결)

✅ Code Generation 계획 (Pure FreeLang)
   - x86-64 명령어 매핑
   - 레지스터 할당 (rbx, rdi, rsi)
   - 런타임 헬퍼 함수 정의
```

---

## Phase 3 Block: 3가지 근본 제약

### Block 1️⃣: **`bytes` 타입 미지원**

**문제:**
```freelang
// 원하는 것:
let elf_header = bytes([0x7f, 0x45, 0x4c, 0x46])

// 현재 가능한 것:
let elf_header = "불가능" // string만 가능
```

**영향:**
- ELF 헤더 (64바이트) 작성 불가
- 바이너리 상수 임베드 불가
- x86-64 기계어 코드 생성 불가

**필요한 기능:**
```freelang
fn bytes_new(size) -> bytes        // 빈 바이트 배열 생성
fn bytes_write(bytes, offset, val) // 특정 위치에 바이트 쓰기
fn bytes_write32(bytes, offset, val) // 32비트 쓰기
fn bytes_write64(bytes, offset, val) // 64비트 쓰기
fn bytes_to_string(bytes) -> string // 바이트를 파일 쓰기용 문자열로
```

---

### Block 2️⃣: **메모리 직접 조작 불가**

**문제:**
```freelang
// 원하는 것:
let ptr = malloc(1024)
ptr[0] = 0x48  // mov rax, ...
ptr[1] = 0x89
ptr[2] = 0xc3  // ret

// 현재: 불가능
```

**영향:**
- 동적 메모리 할당 불가
- 포인터 산술 불가
- 런타임 배열 관리 (array_create/push/get) 불가

**필요한 기능:**
```freelang
fn malloc(size) -> pointer           // 메모리 할당
fn free(ptr)                         // 메모리 해제
fn ptr_write(ptr, offset, val)       // 포인터로 메모리 쓰기
fn ptr_read(ptr, offset) -> int      // 포인터로 메모리 읽기
fn ptr_add(ptr, offset) -> pointer   // 포인터 산술
```

---

### Block 3️⃣: **복잡한 바이너리 연산 어려움**

**문제:**
```freelang
// x86-64 명령어 인코딩 예시
// mov rdi, 0x400000   →  48 BF 00 00 40 00
// call array_create  →  E8 <rel32>
// 이런 바이트 시퀀스를 FreeLang에서 생성 불가능

// 현재 가능한 것:
let code = "불완전한 어셈블리 시뮬레이션"
```

**영향:**
- ELF 코드 섹션 생성 불가
- 동적 링크 정보 (relocation) 불가
- 자체 컴파일러가 바이너리 생성 불가

**필요한 기능:**
```freelang
// 비트 연산
fn bit_and(a, b) -> int
fn bit_or(a, b) -> int
fn bit_shl(val, bits) -> int    // 시프트
fn bit_shr(val, bits) -> int

// 인코딩
fn encode_x86_mov_imm(reg, imm64) -> bytes   // mov reg, imm64
fn encode_x86_call_rel(offset) -> bytes      // call rel32
fn encode_x86_ret() -> bytes                  // ret
```

---

## 해결 방안 (우선순위)

### Level 1️⃣: **VM에 `bytes` 타입 추가** (가장 중요)

**구현 위치**: `src/stdlib.c` + `include/runtime.h`

```c
// runtime.h
typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} fl_bytes_t;

typedef enum {
    // ... 기존 타입들 ...
    FL_TYPE_BYTES,
} fl_type_t;

// 새로운 union 필드
union fl_data {
    // ... 기존 필드들 ...
    fl_bytes_t *bytes_val;  // bytes 타입
};
```

**필요한 builtin 함수**:
- `bytes_new(size)` → FL_TYPE_BYTES 반환
- `bytes_len(bytes)` → 크기 반환
- `bytes_set(bytes, idx, val)` → 특정 위치에 바이트 쓰기
- `bytes_get(bytes, idx)` → 특정 위치 바이트 읽기
- `bytes_write_u32(bytes, offset, val)` → 32비트 쓰기 (endian aware)
- `bytes_write_u64(bytes, offset, val)` → 64비트 쓰기

**예상 코드량**: ~300-500 줄 (C)

---

### Level 2️⃣: **기본 메모리 함수 추가**

**구현 위치**: `src/stdlib.c`

```c
// 메모리 풀 관리
static uint8_t heap[1024*1024];  // 1MB 임시 힙
static size_t heap_ptr = 0;

builtin_malloc(size_t) {
    // 간단한 첫 적합 할당
    uint8_t *ptr = &heap[heap_ptr];
    heap_ptr += size;
    return (intptr_t)ptr;  // 정수로 표현
}

builtin_free(intptr_t) {
    // 간단 버전: NOP (GC 없음)
}
```

**예상 코드량**: ~200 줄 (C)

---

### Level 3️⃣: **x86-64 인코딩 헬퍼** (FreeLang에서 구현 가능)

**구현 위치**: `freelang-compiler-v4-codegen.free`

```freelang
fn encode_mov_rdi_imm64(value) -> bytes {
    // mov rdi, imm64
    // 48 BF <64비트 값 (little-endian)>
    // 총 10바이트
    let b = bytes_new(10)
    bytes_set(b, 0, 0x48)
    bytes_set(b, 1, 0xbf)
    // value를 little-endian으로 쓰기
    bytes_write_u64(b, 2, value)
    return b
}

fn encode_call_rel32(offset) -> bytes {
    // call rel32
    // E8 <offset-5 as little-endian 32-bit>
    let b = bytes_new(5)
    bytes_set(b, 0, 0xe8)
    bytes_write_u32(b, 1, offset)
    return b
}
```

**예상 코드량**: ~100-200 줄 (FreeLang)

---

## 구현 체크리스트

- [ ] **Step 1**: `bytes` 타입을 VM runtime에 추가 (C)
- [ ] **Step 2**: `bytes_*` builtin 함수 구현 (C)
- [ ] **Step 3**: `malloc`/`free` 구현 (C, 간단 버전)
- [ ] **Step 4**: x86-64 인코딩 헬퍼 (FreeLang)
- [ ] **Step 5**: freelang-compiler-v4-codegen.free 업데이트
- [ ] **Step 6**: ELF 바이너리 생성 테스트
- [ ] **Step 7**: self-compile 검증 (v1 → v2 → v3)

---

## 성공 기준

```
✅ freelang-compiler-v4-codegen.free 실행
   → bytes 생성, x86-64 인코딩
   → ELF 바이너리 출력

✅ qemu-x86_64 test-compiler.elf
   → 출력: 2 / hello / world

✅ self-hosting 검증
   → v1.elf (JS로 생성) 실행
   → v2.elf (v1이 생성) 생성
   → MD5(v1.elf) == MD5(v2.elf) ✅
```

---

## 추정 작업 시간

- **Step 1-3** (C 구현): 2-3 시간
- **Step 4-5** (FreeLang): 1 시간
- **Step 6-7** (테스트 + 디버그): 2-3 시간

**총 예상**: 5-7 시간 (고집중도)

---

## 다음 액션

**선택지:**

1. **"Step 1부터 시작합시다"** (C에서 bytes 타입 추가)
   → 가장 중요하고 기초적인 작업

2. **"Step 4부터 준비합시다"** (x86-64 인코더 설계)
   → 병렬 작업 가능

3. **"다른 접근 검토"**
   → 예: JavaScript로 모든 바이너리 생성 (더 빠를 수도)

---

**저는 당신의 지시를 기다리고 있습니다!** 🚀
