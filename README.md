# FreeLang C Runtime v2.0

**외부 의존성 0개** — 완전한 프로그래밍 언어 런타임 + 보안/암호화/이미지/프로세스 관리 내장

동적 타입, 일급 함수, 클로저, 예외 처리, Reactive 상태 관리를 지원하는 Stack-based 인터프리터.
npm, pip, gem 없이 언어 자체가 인프라입니다.

![Build](https://img.shields.io/badge/build-passing-brightgreen)
![Version](https://img.shields.io/badge/version-2.0.0-blue)
![Binary](https://img.shields.io/badge/binary-188KB-orange)
![Deps](https://img.shields.io/badge/dependencies-0-success)
![License](https://img.shields.io/badge/license-MIT-blue)

---

## 핵심 특징

| 항목 | 내용 |
|------|------|
| **바이너리 크기** | 188KB |
| **외부 의존성** | 0개 |
| **총 코드** | 16,000+ 줄 |
| **Opcode** | 50개 |
| **표준 라이브러리** | 130+ 함수 |
| **대체한 npm 패키지** | helmet, bcrypt, sharp, pm2, cluster |

---

## 빠른 시작

```bash
git clone https://gogs.dclub.kr/kim/freelang-c-final.git
cd freelang-c
make clean && make

./bin/fl examples/secure-server.free    # HTTP 보안 서버
./bin/fl examples/fibonacci.fl          # 재귀
./bin/fl examples/closure.fl            # 클로저
```

---

## Phase별 구현 현황

### Phase 1-3: 언어 코어
- 완전한 파이프라인: Lexer → Parser → Compiler → Stack-based VM
- 함수, 재귀, 람다, 고차함수 (map/filter/reduce)
- 예외 처리: try/catch/finally
- Mark-and-Sweep GC + Deep Copy 메모리 안전성

### Phase 4: MOSS-State Reactive
MOSS-State Zero-Copy 상태 관리 컴파일러 통합.

```freelang
reactive let store = { count: 0, messages: [] };

@watch
fn onStateChange() {
    print("State changed!");
}

@transaction
fn addMessage(text) {
    store.messages.push(text);
    return true;
}
```

| 항목 | 목표 | 달성 |
|------|------|------|
| 읽기 | < 1μs | **0.169μs** |
| 쓰기 | < 100μs | **0.865μs** |
| 옵저버 | < 10ms | **0.36ms** |
| 동시 TX | 1000+ | **1000/1000** |

### Phase 5: Crypto — bcrypt 대체

외부 bcrypt/openssl 없이 FreeLang 런타임에 내장된 암호화 엔진.

```freelang
let data = "hello world"
let hash = sha256(data)               // SHA-256 (FIPS 180-4)
let hex  = bytes_to_hex(hash)         // "b94d27b9..."

let key  = crypto_random(32)          // CSPRNG (/dev/urandom)
let mac  = hmac_sha256(key, data)     // HMAC-SHA256 (RFC 2104)

let pw   = "my_password"
let salt = crypto_random(16)
let dk   = pbkdf2(pw, salt, 10000, 32) // PBKDF2-HMAC-SHA256 (RFC 8018)
```

| 함수 | 대체 |
|------|------|
| `sha256()` | crypto.createHash('sha256') |
| `hmac_sha256()` | crypto.createHmac('sha256') |
| `pbkdf2()` | bcrypt / argon2 |
| `crypto_random()` | crypto.randomBytes() |
| `crypto_compare()` | crypto.timingSafeEqual() |

### Phase 6: HTTP Secure-Pipeline — helmet 대체

외부 helmet npm 패키지 없이 모든 HTTP 응답에 보안 헤더 자동 주입.

```freelang
fn main() {
    let body = "FreeLang Secure Server"

    // 별도의 helmet() 호출 없이 7가지 보안 헤더 자동 삽입
    let resp = http_response(200, body)
    println(resp)

    // JSON API 응답
    let json = "{\"status\":\"ok\"}"
    let api_resp = http_response_json(200, json)

    // 커스텀 CSP 정책
    let csp = http_csp("'self'", "'self' https://cdn.example.com")

    // HSTS 설정
    let hsts = http_hsts(31536000)
}

main()
```

자동 삽입되는 보안 헤더 (`http_response()` 호출 시):
```
X-Frame-Options: SAMEORIGIN
X-Content-Type-Options: nosniff
Referrer-Policy: strict-origin-when-cross-origin
X-XSS-Protection: 1; mode=block
Content-Security-Policy: default-src 'self'
Strict-Transport-Security: max-age=31536000; includeSubDomains
Permissions-Policy: camera=(), microphone=(), geolocation=()
```

| 함수 | 대체 |
|------|------|
| `http_secure_headers()` | helmet() |
| `http_csp(...)` | helmet.contentSecurityPolicy() |
| `http_hsts(max_age)` | helmet.hsts() |
| `http_response(status, body)` | res.send() + helmet 미들웨어 |
| `http_response_json(status, body)` | res.json() + helmet 미들웨어 |

### Phase 7: Vector-Vision — sharp 대체

외부 sharp/imagemagick 없이 이미지 처리 내장.

```freelang
let img = vision_load("photo.raw", 640, 480, 3)
let w   = vision_width(img)
let h   = vision_height(img)
let gray = vision_grayscale(img)
let small = vision_resize(img, 320, 240)
let blurred = vision_blur(img, 2)
vision_save(small, "thumbnail.raw")
```

| 함수 | 대체 |
|------|------|
| `vision_load()` | sharp() |
| `vision_resize()` | sharp().resize() |
| `vision_grayscale()` | sharp().grayscale() |
| `vision_blur()` | sharp().blur() |
| `vision_pixel_get/set()` | Canvas API |

### Phase 8: Phoenix-Spawn — pm2/cluster 대체

외부 pm2, cluster 없이 프로세스 관리 내장.

```freelang
let pid = process_spawn("worker", "worker.free", true)
let procs = process_list()
process_restart("worker")
process_kill(pid)

let metrics = system_metrics()
let workers = cluster_workers(4)
```

| 함수 | 대체 |
|------|------|
| `process_spawn()` | pm2 start |
| `process_list()` | pm2 list |
| `process_restart()` | pm2 restart |
| `cluster_workers()` | cluster.fork() |
| `system_metrics()` | os 모듈 |

---

## 언어 문법

### 기본

```freelang
let x = 42
let y = 3.14
println(x + y)

fn add(a, b) {
    return a + b
}
println(add(5, 3))
```

### 재귀

```freelang
fn fibonacci(n) {
    if (n <= 1) return n;
    return fibonacci(n-1) + fibonacci(n-2);
}
println(fibonacci(10))  // 55
```

### 람다 & 고차함수

```freelang
let arr = [1, 2, 3, 4, 5]
let doubled  = map(arr, fn(x) { return x * 2; })
let filtered = filter(arr, fn(x) { return x > 2; })
let sum      = reduce(arr, fn(acc, x) { return acc + x; })
```

### 클로저

```freelang
fn counter() {
    let n = 0
    return fn() {
        n = n + 1
        return n
    }
}
let c = counter()
println(c())  // 1
println(c())  // 2
```

### 예외 처리

```freelang
fn divide(a, b) {
    if (b == 0) throw "Division by zero!"
    return a / b
}

try {
    println(divide(10, 2))
    println(divide(10, 0))
} catch (e) {
    println("Error: " + e)
}
```

---

## 프로젝트 구조

```
freelang-c/
├── include/
│   ├── freelang.h        # 메인 헤더 (타입, Opcode)
│   ├── http_secure.h     # Secure-Pipeline (Phase 6)
│   ├── process.h         # Phoenix-Spawn (Phase 8)
│   ├── cluster.h         # Cluster Manager (Phase 8)
│   ├── logger.h          # 비동기 로거
│   ├── introspect.h      # 런타임 인트로스펙션
│   ├── ast.h, token.h, vm.h, ...
│
├── src/
│   ├── main.c            # 진입점
│   ├── lexer.c           # 토크나이저
│   ├── parser.c          # 파서
│   ├── compiler.c        # 컴파일러
│   ├── vm.c              # 스택 VM (Opcode 50개)
│   ├── runtime.c         # 런타임
│   ├── stdlib.c          # 표준 라이브러리 (130+ 함수)
│   ├── http_secure.c     # HTTP 보안 헤더 (Phase 6)
│   ├── process.c         # 프로세스 관리 (Phase 8)
│   ├── cluster.c         # 클러스터 (Phase 8)
│   ├── logger.c          # 비동기 로거
│   ├── introspect.c      # 인트로스펙션
│   ├── gc.c, ast.c, closure.c, error.c, token.c, typechecker.c
│
├── examples/
│   ├── secure-server.free   # HTTP 보안 서버 (Phase 6)
│   ├── fibonacci.fl, closure.fl, arrays.fl, ...
│
├── Makefile
└── bin/fl                # 실행파일 (188KB, 의존성 0)
```

---

## 빌드 요구사항

- GCC 9+ 또는 Clang 10+
- GNU Make 4.0+
- Linux / macOS

```bash
make              # 기본 빌드
make clean all    # 클린 빌드
make debug        # 디버그 빌드
make release      # 릴리즈 빌드 (-O3)
```

---

## 저장소

**Gogs**: https://gogs.dclub.kr/kim/freelang-c-final

---

**Version**: 2.0.0
**Status**: Production Ready
**Updated**: 2026-03-08
