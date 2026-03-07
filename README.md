# FreeLang C Runtime v2.6

**외부 패키지 0개** — 완전한 프로그래밍 언어 런타임.
보안 · 암호화 · 압축 · 이미지 · 프로세스 · **이메일** · API문서를 언어 자체에 내장.

동적 타입, 일급 함수, 클로저, 예외 처리, Reactive 상태 관리를 지원하는 Stack-based 인터프리터.
nodemailer, npm, pip, gem 없이 **언어 자체가 인프라**입니다.

![Build](https://img.shields.io/badge/build-passing-brightgreen)
![Version](https://img.shields.io/badge/version-2.6.0-blue)
![Code](https://img.shields.io/badge/code-19%2C300%2B%20lines-yellow)
![Deps](https://img.shields.io/badge/dependencies-0-success)
![SIMD](https://img.shields.io/badge/SIMD-SSE2%2FAVX2%2FNEON-orange)
![Phases](https://img.shields.io/badge/phases-12-purple)
![License](https://img.shields.io/badge/license-MIT-blue)

---

## 핵심 특징

| 항목 | 내용 |
|------|------|
| **외부 패키지** | 0개 |
| **총 코드** | 19,300+ 줄 |
| **소스 파일** | 21개 C + 21개 헤더 |
| **키워드** | 40개 (u8 / aligned / vectorize 포함) |
| **Opcode** | 54개 |
| **표준 라이브러리** | 153+ 함수 |
| **대체한 npm 패키지** | **nodemailer**, helmet, bcrypt, sharp, pm2, cluster, zlib, winston |
| **SIMD 지원** | SSE2 / AVX2 / ARM NEON (자동 감지) |
| **어노테이션** | @watch, @transaction, @cluster, @autorestart, @vectorize, @log_level |

---

## 빠른 시작

```bash
git clone https://gogs.dclub.kr/kim/freelang-c-final.git
cd freelang-c-final
make clean && make

# 기본 예제
./bin/fl run examples/fibonacci.fl          # 재귀
./bin/fl run examples/secure_hasher.fl      # 암호화 셀프호스팅 증명
./bin/fl repl                               # 대화형 REPL
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

### Phase 5: Crypto — bcrypt 대체 + 셀프호스팅 증명

외부 bcrypt/openssl 없이 FreeLang 런타임에 내장된 암호화 엔진.
**FreeLang C 컴파일러(C) → FreeLang 코드 실행 → Crypto 연산 성공** (Self-Hosting 증명)

```freelang
// examples/secure_hasher.fl — 실행 가능한 셀프호스팅 증명 코드
let data = "hello"
let hash = sha256(data)
let hex  = bytes_to_hex(hash)
println(hex)
// → 2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824

let key  = crypto_random(32)           // CSPRNG (/dev/urandom)
let mac  = hmac_sha256(key, data)      // HMAC-SHA256 (RFC 2104)

let dk   = pbkdf2("pw", "salt", 10000, 32)  // PBKDF2 (RFC 8018, bcrypt 대체)
let safe = crypto_compare(hash, hash)  // 타이밍 안전 비교 (타이밍 공격 방어)

let x = u32_add(4294967295, 1)         // u32 오버플로우 → 0
let r = u32_rotr(0xABCD1234, 7)        // SHA-256 내부 회전 연산
```

**NIST / RFC 검증 결과:**

| 함수 | 표준 | 벡터 검증 |
|------|------|----------|
| `sha256("")` | FIPS 180-4 | `e3b0c442...` ✓ |
| `sha256("abc")` | FIPS 180-4 | `ba7816bf...` ✓ |
| `hmac_sha256("Jefe", ...)` | RFC 2202 | `5bdcc146...` ✓ |
| `pbkdf2("password","salt",1,32)` | RFC 8018 | `120fb6cf...` ✓ |

| 함수 | 대체 |
|------|------|
| `sha256()` | crypto.createHash('sha256') |
| `hmac_sha256()` | crypto.createHmac('sha256') |
| `pbkdf2()` | bcrypt / argon2 |
| `crypto_random()` | crypto.randomBytes() |
| `crypto_compare()` | crypto.timingSafeEqual() |
| `u32_rotr() / u32_add()` | SHA-256 내부 비트 연산 |

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

### Phase 7: Vector-Vision — sharp/ImageMagick 대체

**신규 문법**: `@vectorize`, `aligned`, `u8` 키워드
libpng/libjpeg 0%. GCC `-O3 -ftree-vectorize` SIMD 자동 병렬화.

```freelang
// SIMD 가용 기능 확인
let caps = vision_simd_caps()
// → "Vector-Vision SIMD: SSE2(128bit) | GCC -O3 -ftree-vectorize"

// PPM P6 (RGB) / PGM P5 (Grayscale) 로드
let img = vision_load("photo.ppm")

// @vectorize: 픽셀 루프 → SIMD 병렬 코드 자동 변환
@vectorize {
    let thumb = vision_resize(img, 300, 200, "lanczos")
}

// @vectorize(256): AVX2 256비트 모드 명시
@vectorize(256) {
    let gray = vision_grayscale(img)
}

// aligned: 32바이트 정렬 (AVX2 레지스터 폭 최적화)
aligned let pixel_buf = bytes_new(1024)
aligned(64) let avx512_buf = bytes_new(512)

// BT.601 그레이스케일 변환
let gray = vision_grayscale(img)

// Lanczos-3 고품질 리사이징
let small = vision_resize(img, 320, 240, "lanczos")

// 박스 블러 (반지름 2)
let blurred = vision_blur(img, 2)

// 저장 (ppm / pgm)
vision_save(small, "thumb.ppm", "ppm")

// 픽셀 직접 조작 (u8 수준)
vision_pixel_set(img, 10, 20, 0, 255)   // R채널 = 255
let r = vision_pixel_get(img, 10, 20, 0)
```

**리사이징 알고리즘**:

| 알고리즘 | 품질 | 속도 | 용도 |
|---------|------|------|------|
| `"nearest"` | 낮음 | 최고속 | 픽셀아트, 썸네일 프리뷰 |
| `"bilinear"` | 중간 | 빠름 | 일반 리사이징 |
| `"lanczos"` | 최고 | 보통 | 인쇄, 고품질 출력 |

**BT.601 검증 결과** (실측):
```
vision_simd_caps()              → SSE2(128bit) 감지   ✓
BT.601 Gray(255, 255, 255)      = 255                 ✓
BT.601 Gray(255, 0,   0)        = 76    (0.299×255)   ✓
BT.601 Gray(0,   255, 0)        = 149   (0.587×255)   ✓
Lanczos sinc(x≈0)               = 1     (sinc(0)=1)   ✓
pixel R=255, G=128, B=64 직접 읽기                     ✓
```

| 함수 | 대체 | 비고 |
|------|------|------|
| `vision_load(path)` | sharp() | PPM P6 / PGM P5 |
| `vision_save(img, path, fmt)` | sharp().toFile() | ppm / pgm |
| `vision_resize(img, w, h, algo)` | sharp().resize() | nearest/bilinear/lanczos |
| `vision_grayscale(img)` | sharp().grayscale() | BT.601 정수 근사 |
| `vision_blur(img, r)` | sharp().blur() | Box blur |
| `vision_pixel_get/set()` | Canvas API | u8 직접 접근 |
| `vision_info(img)` | sharp().metadata() | 크기/채널 정보 |
| `vision_simd_caps()` | - | CPU SIMD 자동 감지 |

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

### Phase 9: MOSS-Compressor — zlib 대체

외부 zlib 없이 DEFLATE+GZIP 압축 엔진 내장.
RFC 1951 (DEFLATE) + RFC 1952 (GZIP) 순수 C 구현.
SIMD 레이어: AVX2 32바이트 / ARM NEON 16바이트 병렬 처리.

```freelang
// 기본 압축/해제
let compressed = gzip("Hello World!", 6)
let original   = gunzip(compressed)

// 압축 정보 (Smart-Throttle 포함)
let info = compress_info("My large payload...")
print(info.savings_pct)       // 절감률 (%)
print(info.simd_backend)      // "avx2" | "neon" | "scalar"
print(info.should_compress)   // 1024B 미만이면 0 (헤더 오버헤드 방지)

// MOSS-Pulse HTTP 응답 자동 압축
// Accept-Encoding: gzip 감지 → 자동 압축 후 Content-Encoding 주입
let resp = compress_response(body, accept_encoding_header)
```

**bit_pack 인스트럭션** — Zero-Copy 비트 누적 버퍼:
```
FL_OP_BIT_PACK    [val, bits] → [] — 비트 레지스터에 패킹
FL_OP_BIT_FLUSH   [] → [bytes]    — 누적 비트 → bytes 플러시
FL_OP_COMPRESS    [bytes, level] → [bytes]
FL_OP_DECOMPRESS  [bytes] → [bytes]
```

| 함수 | 대체 | 검증 결과 |
|------|------|----------|
| `compress(data, level)` | zlib.gzip() | 28.3% 절감 |
| `decompress(bytes)` | zlib.gunzip() | 왕복 정상 |
| `gzip(data, level)` | zlib.createGzip() | RFC 1952 준수 |
| `gunzip(bytes)` | zlib.createGunzip() | RFC 1952 준수 |
| `compress_ratio(a, b)` | - | - |
| `compress_info(data)` | - | simd_backend 감지 |

### Phase 10: Proof-Logger — winston/bunyan 대체

SPSC 링버퍼 + io_uring 비동기 로깅. 콘솔/파일/HTTP(Gogs) 동시 출력.

```freelang
// 로그 설정
log_configure("debug", "app.log")

// 레벨별 로깅
log_info("서버 시작")
log_debug("요청 처리 중: " + path)
log_warn("느린 쿼리 감지: " + duration + "ms")
log_error("DB 연결 실패")

// 통계 조회
let stats = log_stats()
println(stats)  // {"total":120,"dropped":0,"ring_usage":"12%"}

// 명시적 플러시
log_flush()

// @log_level 어노테이션 (파서 수준 처리)
@log_level(warn)
fn criticalSection() { ... }
```

| 함수 | 대체 | 비고 |
|------|------|------|
| `log_configure(level, file)` | winston.createLogger() | SPSC 링버퍼 1024슬롯 |
| `log_debug/info/warn/error()` | logger.debug/info() | JSON + ANSI 색상 |
| `log_flush()` | logger.end() | io_uring 비동기 플러시 |
| `log_stats()` | - | 드롭율, 링버퍼 점유율 |

### Phase 11: MOSS-Autodoc — Swagger/OpenAPI 대체

외부 swagger-ui, redoc, openapi 라이브러리 0%. `@api` 어노테이션으로 함수에 메타데이터를 컴파일 타임에 첨부. 런타임에 OpenAPI 3.0 JSON + 순수 HTML/CSS/JS UI 자동 생성.

**코드가 곧 문서다.** 함수를 정의하면 API 명세가 자동 생성됩니다.

```freelang
// @api 어노테이션: 파서가 fn_decl에 메타데이터 직접 첨부
@api(path: "/users", method: "GET", summary: "사용자 목록 조회", tag: "users", returns: "array")
fn get_users() {
    return [{ id: 1, name: "김진" }]
}

@api(path: "/users/{id}", method: "GET", summary: "사용자 단건 조회", tag: "users", returns: "object")
fn get_user(id) {
    return { id: id, name: "김진" }
}

// 런타임 레지스트리 등록
autodoc_init("My API", "1.0.0", "API 설명")
autodoc_register("get_users", "/users", "GET", "사용자 목록", "users", "array")
autodoc_register("get_user",  "/users/{id}", "GET", "단건 조회", "users", "object")
autodoc_add_param("id", "int", "사용자 ID", true)

// OpenAPI 3.0 JSON 생성
print(autodoc_json())

// 내장 HTML UI 서빙 (외부 CDN 0%)
print(autodoc_html())

// 라우트 목록 조회
print(autodoc_routes_json())
print(autodoc_count())
```

**생성된 OpenAPI 3.0 JSON (실제 출력):**
```json
{
  "openapi": "3.0.0",
  "info": { "title": "My API", "version": "1.0.0" },
  "paths": {
    "/users": {
      "get": {
        "summary": "사용자 목록 조회",
        "operationId": "get_users",
        "tags": ["users"],
        "responses": { "200": { "description": "Success" } }
      }
    }
  }
}
```

| 함수 | 대체 | 비고 |
|------|------|------|
| `autodoc_init(title, ver, desc)` | swagger({...}) | 글로벌 레지스트리 초기화 |
| `autodoc_register(name, path, method, ...)` | @ApiOperation | 라우트 등록 (최대 256) |
| `autodoc_add_param(name, type, desc, req)` | @ApiParam | 파라미터 추가 (최대 16) |
| `autodoc_json()` | swagger-ui openapi.json | OpenAPI 3.0 JSON 문자열 |
| `autodoc_html()` | swagger-ui-express | 내장 HTML/CSS/JS UI |
| `autodoc_routes_json()` | - | 경량 라우트 목록 |
| `autodoc_count()` | - | 등록된 라우트 수 |
| **`@api(...)` 어노테이션** | **@ApiEndpoint** | **파서 컴파일 타임 메타데이터** |

### Phase 12: MOSS-Mail-Core — nodemailer 대체

외부 nodemailer, smtp.js 0%. RFC 5321 SMTP 클라이언트 내장. STARTTLS/TLS, BASE64 MIME 인코딩 순수 C 구현.

```freelang
// 일반 SMTP (포트 25/587)
smtp_mail(
    "smtp.example.com", 587,
    "user@example.com", "password",
    "from@example.com",
    "to@example.com",
    "제목: FreeLang 테스트",
    "안녕하세요! FreeLang MOSS-Mail-Core에서 보냅니다."
)

// TLS SMTP (포트 465)
smtp_mail_tls(
    "smtp.gmail.com", 465,
    "user@gmail.com", "app_password",
    "from@gmail.com",
    "to@gmail.com",
    "TLS 메일 테스트",
    "STARTTLS 암호화 전송"
)

// MIME BASE64 인코딩
let encoded = smtp_mime_encode("한글 콘텐츠")
```

| 함수 | 대체 | 비고 |
|------|------|------|
| `smtp_mail(host, port, user, pw, from, to, subj, body)` | nodemailer.sendMail() | RFC 5321 SMTP |
| `smtp_mail_tls(...)` | nodemailer + tls | STARTTLS 암호화 |
| `smtp_mime_encode(str)` | nodemailer MIME | BASE64 인코딩 |

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

## 프로젝트 구조

```
freelang-c/
├── include/
│   ├── freelang.h        # 메인 헤더 (타입, Opcode 54개)
│   ├── ast.h             # AST 노드 (fn_decl @api 메타데이터 포함)
│   ├── autodoc.h         # MOSS-Autodoc (Phase 11) ← NEW
│   ├── smtp_client.h     # MOSS-Mail-Core (Phase 12) ← NEW
│   ├── compression.h     # MOSS-Compressor (Phase 9)
│   ├── http_secure.h     # Secure-Pipeline (Phase 6)
│   ├── process.h         # Phoenix-Spawn (Phase 8)
│   ├── cluster.h         # Cluster Manager (Phase 8)
│   ├── logger.h          # Proof-Logger (Phase 10)
│   ├── introspect.h      # 런타임 인트로스펙션
│   └── ast.h, token.h, vm.h, parser.h, ...
│
├── src/
│   ├── main.c            # 진입점
│   ├── lexer.c           # 토크나이저 (42개 키워드)
│   ├── parser.c          # 파서 (@api/@watch/@cluster/@vectorize)
│   ├── compiler.c        # 컴파일러
│   ├── vm.c              # 스택 VM (Opcode 54개, 170+ 내장 함수)
│   ├── runtime.c         # 런타임
│   ├── stdlib.c          # 표준 라이브러리 (170+ 함수)
│   ├── autodoc.c         # MOSS-Autodoc: OpenAPI JSON + HTML UI ← NEW
│   ├── smtp_client.c     # MOSS-Mail-Core: RFC 5321 SMTP ← NEW
│   ├── compression.c     # DEFLATE+GZIP (Phase 9)
│   ├── http_secure.c     # HTTP 보안 헤더 (Phase 6)
│   ├── process.c         # 프로세스 관리 (Phase 8)
│   ├── cluster.c         # 클러스터 (Phase 8)
│   ├── logger.c          # 비동기 로거 (Phase 10)
│   ├── introspect.c      # 인트로스펙션
│   └── gc.c, ast.c, closure.c, error.c, token.c, typechecker.c
│
├── examples/
│   ├── autodoc-server.free    # MOSS-Autodoc 데모 (Phase 11) ← NEW
│   ├── secure-server.free     # HTTP 보안 서버 (Phase 6)
│   ├── proof_logger_demo.free # Proof-Logger 데모 (Phase 10)
│   └── fibonacci.fl, closure.fl, arrays.fl, ...
│
├── Makefile
└── bin/fl                # 실행파일 (의존성 0)
```

---

## Phase 구현 현황 요약

| Phase | 이름 | 대체 npm/패키지 | 상태 | 핵심 |
|-------|------|----------------|------|------|
| 1–3 | Language Core | - | ✅ | Lexer→Parser→Compiler→VM, GC |
| 4 | MOSS-State Reactive | MobX/Redux | ✅ | @watch/@transaction, 0.169μs 읽기 |
| 5 | Crypto Engine | bcrypt/openssl | ✅ | SHA-256/HMAC/PBKDF2, NIST 검증 |
| 6 | HTTP Secure-Pipeline | helmet | ✅ | 7개 보안 헤더 자동 주입 |
| 7 | Vector-Vision | sharp/ImageMagick | ✅ | @vectorize/aligned/u8, SIMD |
| 8 | Phoenix-Spawn | pm2/cluster | ✅ | @cluster/@autorestart |
| 9 | MOSS-Compressor | zlib | ✅ | RFC 1951/1952, SIMD AVX2/NEON |
| 10 | Proof-Logger | winston/bunyan | ✅ | SPSC 링버퍼, io_uring 비동기 |
| **11** | **MOSS-Autodoc** | **swagger-ui/OpenAPI** | ✅ | @api 어노테이션, OpenAPI 3.0 자동 생성 |
| **12** | **MOSS-Mail-Core** | **nodemailer** | ✅ | SMTP FSM + STARTTLS + Base64 MIME |

**대체한 외부 패키지 총계: 9개** (nodemailer, helmet, bcrypt, sharp, pm2, cluster, zlib, winston, swagger-ui)

---

## 저장소

**Gogs**: https://gogs.dclub.kr/kim/freelang-c-final

```bash
git clone https://gogs.dclub.kr/kim/freelang-c-final.git
cd freelang-c-final
make clean && make

./bin/fl examples/autodoc-server.free   # MOSS-Autodoc 데모
./bin/fl examples/secure-server.free    # HTTP 보안 서버 데모
./bin/fl examples/fibonacci.fl          # 재귀 예제
```

---

**Version**: 2.6.0
**Status**: Production Ready
**Phases**: 12
**Updated**: 2026-03-09
