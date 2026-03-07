/**
 * FreeLang MOSS-Autodoc Engine (Phase 8)
 * Self-Documenting API System - Swagger/OpenAPI 대체 네이티브 구현
 * 외부 의존성 0% - 순수 C
 */

#ifndef FL_AUTODOC_H
#define FL_AUTODOC_H

#include <stddef.h>

/* ============================================================
   상수
   ============================================================ */
#define FL_AUTODOC_MAX_ROUTES   256
#define FL_AUTODOC_MAX_PARAMS   16
#define FL_AUTODOC_PATH_LEN     512
#define FL_AUTODOC_SUMMARY_LEN  1024
#define FL_AUTODOC_NAME_LEN     256
#define FL_AUTODOC_TYPE_LEN     64

/* ============================================================
   파라미터 메타데이터
   ============================================================ */
typedef struct {
    char name[FL_AUTODOC_NAME_LEN];
    char type[FL_AUTODOC_TYPE_LEN];     /* "string", "int", "bool", "object" */
    char desc[FL_AUTODOC_SUMMARY_LEN];
    int  required;                       /* 1 = 필수, 0 = 선택 */
} fl_api_param_t;

/* ============================================================
   API 라우트 메타데이터
   ============================================================ */
typedef struct {
    char name[FL_AUTODOC_NAME_LEN];         /* FreeLang 함수 이름 */
    char path[FL_AUTODOC_PATH_LEN];         /* HTTP 경로: "/users/{id}" */
    char method[16];                        /* "GET", "POST", "PUT", "DELETE", "PATCH" */
    char summary[FL_AUTODOC_SUMMARY_LEN];   /* 요약 설명 */
    char description[FL_AUTODOC_SUMMARY_LEN]; /* 상세 설명 */
    char returns[FL_AUTODOC_TYPE_LEN];      /* 반환 타입 */
    char tag[FL_AUTODOC_NAME_LEN];          /* 그룹 태그 */
    fl_api_param_t params[FL_AUTODOC_MAX_PARAMS];
    int  param_count;
} fl_api_route_t;

/* ============================================================
   글로벌 레지스트리
   ============================================================ */
typedef struct {
    fl_api_route_t routes[FL_AUTODOC_MAX_ROUTES];
    int  count;
    char title[FL_AUTODOC_NAME_LEN];    /* API 문서 제목 */
    char version[32];                   /* API 버전 */
    char description[FL_AUTODOC_SUMMARY_LEN];
    char base_url[FL_AUTODOC_PATH_LEN];
} fl_autodoc_registry_t;

/* 전역 레지스트리 (autodoc.c에서 정의) */
extern fl_autodoc_registry_t fl_autodoc_global;

/* ============================================================
   공개 API
   ============================================================ */

/* 초기화 */
void fl_autodoc_init(const char *title, const char *version, const char *description);

/* 라우트 등록 */
int fl_autodoc_register(const char *name, const char *path, const char *method,
                        const char *summary, const char *tag, const char *returns);

/* 파라미터 추가 (마지막으로 등록된 라우트에) */
int fl_autodoc_add_param(const char *name, const char *type,
                         const char *desc, int required);

/* OpenAPI 3.0 JSON 생성 (malloc, 호출자가 free) */
char *fl_autodoc_to_openapi_json(void);

/* HTML UI 렌더링 - 내장 순수 HTML/CSS/JS (malloc, 호출자가 free) */
char *fl_autodoc_render_html(void);

/* 간단한 요약 JSON (라우트 목록) */
char *fl_autodoc_routes_json(void);

#endif /* FL_AUTODOC_H */
