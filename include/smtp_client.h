/**
 * FreeLang SMTP Client - MOSS-Mail-Core
 *
 * nodemailer 없이 FreeLang 런타임에 내장된 SMTP 상태 머신.
 * OpenSSL STARTTLS 지원 (시스템 라이브러리, 외부 패키지 0개)
 *
 * 설계:
 *  - 순수 POSIX TCP 소켓 + FSM (Finite State Machine)
 *  - Base64 인코딩: 순수 C (SIMD 힌트 포함)
 *  - STARTTLS: OpenSSL SSL_write/SSL_read 래퍼
 *  - MIME 멀티파트: Content-Transfer-Encoding: base64
 */

#ifndef FL_SMTP_CLIENT_H
#define FL_SMTP_CLIENT_H

#include <stddef.h>
#include "freelang.h"

/* ============================================================
   SMTP FSM 상태 코드
   ============================================================ */
typedef enum {
    SMTP_STATE_INIT        = 0,
    SMTP_STATE_CONNECTED   = 1,   /* 220 greeting 수신 */
    SMTP_STATE_EHLO_OK     = 2,   /* 250 EHLO 완료 */
    SMTP_STATE_TLS_OK      = 3,   /* STARTTLS 업그레이드 완료 */
    SMTP_STATE_AUTH_OK     = 4,   /* AUTH LOGIN 성공 */
    SMTP_STATE_MAIL_FROM   = 5,   /* MAIL FROM 전송 후 */
    SMTP_STATE_RCPT_TO     = 6,   /* RCPT TO 전송 후 */
    SMTP_STATE_DATA_OK     = 7,   /* DATA 354 수신, 본문 전송 가능 */
    SMTP_STATE_SENT        = 8,   /* 250 메일 전송 완료 */
    SMTP_STATE_ERROR       = 99   /* 오류 상태 */
} smtp_state_t;

/* SMTP 응답 코드 범위 */
#define SMTP_CODE_READY        220
#define SMTP_CODE_BYE          221
#define SMTP_CODE_AUTH_OK      235
#define SMTP_CODE_OK           250
#define SMTP_CODE_AUTH_CONT    334  /* AUTH 계속 (base64 챌린지) */
#define SMTP_CODE_DATA_START   354
#define SMTP_CODE_ERR_START    400  /* 400+ = 오류 */

/* 버퍼 크기 */
#define SMTP_LINE_BUF     4096
#define SMTP_BODY_MAX  (1024 * 1024)   /* 1 MB */
#define SMTP_BASE64_BUF (SMTP_BODY_MAX * 4 / 3 + 16)

/* SMTP 연결 핸들 (불투명 포인터로 노출) */
typedef struct fl_smtp_conn fl_smtp_conn_t;

/* ============================================================
   공개 API
   ============================================================ */

/**
 * smtp_mail(host, port, user, pass, from, to, subject, body) -> string
 * plain TCP SMTP (포트 25 / 587 AUTH LOGIN)
 * 반환: "ok" 또는 "error: [메시지]"
 */
fl_value_t fl_smtp_mail(fl_value_t *args, size_t argc);

/**
 * smtp_mail_tls(host, port, user, pass, from, to, subject, body) -> string
 * STARTTLS 업그레이드 후 TLS 터널에서 AUTH/DATA 수행
 * 반환: "ok" 또는 "error: [메시지]"
 */
fl_value_t fl_smtp_mail_tls(fl_value_t *args, size_t argc);

/**
 * smtp_mime_encode(text) -> string
 * 문자열을 Base64 MIME 인코딩하여 반환 (테스트/디버그용)
 */
fl_value_t fl_smtp_mime_encode(fl_value_t *args, size_t argc);

#endif /* FL_SMTP_CLIENT_H */
