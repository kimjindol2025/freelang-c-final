/**
 * FreeLang HTTP Secure-Pipeline
 *
 * helmet 외부 라이브러리 없이 FreeLang 런타임 수준에서
 * 보안 헤더를 정적 상수로 임베딩하고 자동 주입합니다.
 *
 * 지원 헤더:
 *   X-Frame-Options          (Clickjacking 방어)
 *   X-Content-Type-Options   (MIME 스니핑 방어)
 *   Referrer-Policy          (정보 유출 방어)
 *   X-XSS-Protection         (구형 브라우저 XSS 필터)
 *   Content-Security-Policy  (XSS/인젝션 방어)
 *   Strict-Transport-Security (HTTPS 강제)
 *   Permissions-Policy       (브라우저 API 접근 제한)
 */

#ifndef FL_HTTP_SECURE_H
#define FL_HTTP_SECURE_H

#include "freelang.h"

/* ============================================================
   header_template: 바이너리 수준에서 임베딩된 정적 보안 헤더 상수
   모든 FreeLang HTTP 응답에 자동 삽입되는 기본값
   ============================================================ */

/* 기본 보안 헤더 (Secure-Pipeline 기본값) */
#define FL_HDR_X_FRAME           "X-Frame-Options: SAMEORIGIN"
#define FL_HDR_X_CONTENT_TYPE    "X-Content-Type-Options: nosniff"
#define FL_HDR_REFERRER_POLICY   "Referrer-Policy: strict-origin-when-cross-origin"
#define FL_HDR_XSS_PROTECTION    "X-XSS-Protection: 1; mode=block"
#define FL_HDR_CSP_DEFAULT       "Content-Security-Policy: default-src 'self'"
#define FL_HDR_HSTS_DEFAULT      "Strict-Transport-Security: max-age=31536000; includeSubDomains"
#define FL_HDR_PERMISSIONS       "Permissions-Policy: camera=(), microphone=(), geolocation=()"

/* 응답 헤더 구분자 */
#define FL_HTTP_CRLF             "\r\n"
#define FL_HTTP_HEADER_END       "\r\n\r\n"

/* 최대 헤더 버퍼 크기 (정적 할당으로 오버헤드 최소화) */
#define FL_SECURE_HEADER_BUF     2048
#define FL_CSP_POLICY_BUF        512
#define FL_RESPONSE_BUF          8192

/* ============================================================
   Secure-Pipeline API (FreeLang stdlib에서 호출)
   ============================================================ */

/**
 * fl_http_secure_headers(args, argc) -> fl_value_t (object)
 * FreeLang: http_secure_headers()
 *
 * 7가지 보안 헤더를 key-value object로 반환합니다.
 * 반환값을 그대로 HTTP 응답에 포함하면 됩니다.
 */
fl_value_t fl_http_secure_headers(fl_value_t* args, size_t argc);

/**
 * fl_http_csp(args, argc) -> fl_value_t (string)
 * FreeLang: http_csp(default_src, script_src, style_src)
 *
 * Content-Security-Policy 헤더값을 생성합니다.
 * args[0]: default-src 값 (예: "'self'")
 * args[1]: script-src 값 (선택적, 예: "'self' 'unsafe-inline'")
 * args[2]: style-src 값  (선택적)
 */
fl_value_t fl_http_csp(fl_value_t* args, size_t argc);

/**
 * fl_http_hsts(args, argc) -> fl_value_t (string)
 * FreeLang: http_hsts(max_age)
 *
 * Strict-Transport-Security 헤더값을 생성합니다.
 * args[0]: max-age 초 단위 (int, 기본값 31536000 = 1년)
 */
fl_value_t fl_http_hsts(fl_value_t* args, size_t argc);

/**
 * fl_http_response(args, argc) -> fl_value_t (string)
 * FreeLang: http_response(status, body)
 *
 * 보안 헤더가 자동 삽입된 완전한 HTTP/1.1 응답 문자열을 반환합니다.
 * args[0]: HTTP 상태 코드 (int, 예: 200)
 * args[1]: 응답 본문 (string)
 *
 * 반환 형식:
 *   HTTP/1.1 200 OK\r\n
 *   [7가지 보안 헤더]\r\n
 *   Content-Type: text/plain\r\n
 *   Content-Length: N\r\n
 *   \r\n
 *   [body]
 */
fl_value_t fl_http_response(fl_value_t* args, size_t argc);

/**
 * fl_http_response_json(args, argc) -> fl_value_t (string)
 * FreeLang: http_response_json(status, json_body)
 *
 * Content-Type: application/json으로 설정된 보안 JSON 응답입니다.
 */
fl_value_t fl_http_response_json(fl_value_t* args, size_t argc);

/**
 * fl_http_status_text(code) -> const char*
 * 내부 헬퍼: HTTP 상태 코드를 텍스트로 변환
 */
const char* fl_http_status_text(int code);

#endif /* FL_HTTP_SECURE_H */
