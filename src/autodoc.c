/**
 * FreeLang MOSS-Autodoc Engine (Phase 8)
 * Self-Documenting API System
 *
 * 구현:
 * - 글로벌 API 레지스트리 (최대 256 라우트)
 * - OpenAPI 3.0 JSON 생성
 * - 내장 HTML/CSS/JS UI (외부 CDN 0%)
 * - 외부 의존성: 0
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "../include/autodoc.h"

/* ============================================================
   글로벌 레지스트리 (단일 인스턴스)
   ============================================================ */
fl_autodoc_registry_t fl_autodoc_global = {
    .count   = 0,
    .title   = "FreeLang API",
    .version = "1.0.0",
    .description = "Self-documenting API powered by FreeLang MOSS-Autodoc",
    .base_url = ""
};

/* ============================================================
   초기화
   ============================================================ */
void fl_autodoc_init(const char *title, const char *version, const char *description) {
    if (title)       strncpy(fl_autodoc_global.title, title, FL_AUTODOC_NAME_LEN - 1);
    if (version)     strncpy(fl_autodoc_global.version, version, 31);
    if (description) strncpy(fl_autodoc_global.description, description, FL_AUTODOC_SUMMARY_LEN - 1);
    fl_autodoc_global.count = 0;
}

/* ============================================================
   라우트 등록
   ============================================================ */
int fl_autodoc_register(const char *name, const char *path, const char *method,
                        const char *summary, const char *tag, const char *returns) {
    if (fl_autodoc_global.count >= FL_AUTODOC_MAX_ROUTES) return -1;

    fl_api_route_t *r = &fl_autodoc_global.routes[fl_autodoc_global.count];
    memset(r, 0, sizeof(fl_api_route_t));

    if (name)    strncpy(r->name,    name,    FL_AUTODOC_NAME_LEN - 1);
    if (path)    strncpy(r->path,    path,    FL_AUTODOC_PATH_LEN - 1);
    if (method) {
        strncpy(r->method, method, 15);
        /* 대문자 변환 */
        for (int i = 0; r->method[i]; i++) {
            if (r->method[i] >= 'a' && r->method[i] <= 'z')
                r->method[i] -= 32;
        }
    }
    if (summary) strncpy(r->summary, summary, FL_AUTODOC_SUMMARY_LEN - 1);
    if (tag)     strncpy(r->tag,     tag,     FL_AUTODOC_NAME_LEN - 1);
    if (returns) strncpy(r->returns, returns, FL_AUTODOC_TYPE_LEN - 1);

    r->param_count = 0;
    fl_autodoc_global.count++;
    return fl_autodoc_global.count - 1;  /* 등록된 인덱스 반환 */
}

/* ============================================================
   파라미터 추가 (마지막 라우트에 첨부)
   ============================================================ */
int fl_autodoc_add_param(const char *name, const char *type,
                         const char *desc, int required) {
    if (fl_autodoc_global.count == 0) return -1;
    fl_api_route_t *r = &fl_autodoc_global.routes[fl_autodoc_global.count - 1];
    if (r->param_count >= FL_AUTODOC_MAX_PARAMS) return -1;

    fl_api_param_t *p = &r->params[r->param_count];
    if (name) strncpy(p->name, name, FL_AUTODOC_NAME_LEN - 1);
    if (type) strncpy(p->type, type, FL_AUTODOC_TYPE_LEN - 1);
    if (desc) strncpy(p->desc, desc, FL_AUTODOC_SUMMARY_LEN - 1);
    p->required = required;
    r->param_count++;
    return 0;
}

/* ============================================================
   JSON 이스케이프 헬퍼 (malloc)
   ============================================================ */
static char *json_escape(const char *str) {
    if (!str) return strdup("");
    size_t len = strlen(str);
    char *out = malloc(len * 6 + 3);  /* 최악의 경우 모두 이스케이프 */
    if (!out) return strdup("");

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        if (c == '"')       { out[j++] = '\\'; out[j++] = '"'; }
        else if (c == '\\') { out[j++] = '\\'; out[j++] = '\\'; }
        else if (c == '\n') { out[j++] = '\\'; out[j++] = 'n'; }
        else if (c == '\r') { out[j++] = '\\'; out[j++] = 'r'; }
        else if (c == '\t') { out[j++] = '\\'; out[j++] = 't'; }
        else if (c < 0x20)  { j += sprintf(out + j, "\\u%04x", c); }
        else                { out[j++] = c; }
    }
    out[j] = '\0';
    return out;
}

/* ============================================================
   동적 문자열 버퍼 헬퍼
   ============================================================ */
typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} strbuf_t;

static void sb_init(strbuf_t *sb) {
    sb->cap = 4096;
    sb->len = 0;
    sb->buf = malloc(sb->cap);
    if (sb->buf) sb->buf[0] = '\0';
}

static void sb_append(strbuf_t *sb, const char *str) {
    if (!str || !sb->buf) return;
    size_t add = strlen(str);
    while (sb->len + add + 1 > sb->cap) {
        sb->cap *= 2;
        char *nb = realloc(sb->buf, sb->cap);
        if (!nb) return;
        sb->buf = nb;
    }
    memcpy(sb->buf + sb->len, str, add + 1);
    sb->len += add;
}

static void sb_appendf(strbuf_t *sb, const char *fmt, ...) {
    char tmp[8192];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n > 0) sb_append(sb, tmp);
}

/* ============================================================
   OpenAPI 3.0 JSON 생성
   ============================================================ */
char *fl_autodoc_to_openapi_json(void) {
    strbuf_t sb;
    sb_init(&sb);

    char *title_e   = json_escape(fl_autodoc_global.title);
    char *ver_e     = json_escape(fl_autodoc_global.version);
    char *desc_e    = json_escape(fl_autodoc_global.description);

    sb_append(&sb, "{\n");
    sb_append(&sb, "  \"openapi\": \"3.0.0\",\n");
    sb_appendf(&sb, "  \"info\": {\n    \"title\": \"%s\",\n    \"version\": \"%s\",\n    \"description\": \"%s\"\n  },\n",
               title_e, ver_e, desc_e);

    free(title_e); free(ver_e); free(desc_e);

    /* paths */
    sb_append(&sb, "  \"paths\": {\n");
    for (int i = 0; i < fl_autodoc_global.count; i++) {
        fl_api_route_t *r = &fl_autodoc_global.routes[i];
        char *path_e    = json_escape(r->path);
        char *sum_e     = json_escape(r->summary);
        char *name_e    = json_escape(r->name);
        char *tag_e     = json_escape(r->tag[0] ? r->tag : "default");
        char  method_lc[16] = {0};

        /* 소문자 메서드 */
        strncpy(method_lc, r->method, 15);
        for (int k = 0; method_lc[k]; k++)
            if (method_lc[k] >= 'A' && method_lc[k] <= 'Z')
                method_lc[k] += 32;

        if (i > 0) sb_append(&sb, ",\n");
        sb_appendf(&sb,
            "    \"%s\": {\n"
            "      \"%s\": {\n"
            "        \"summary\": \"%s\",\n"
            "        \"operationId\": \"%s\",\n"
            "        \"tags\": [\"%s\"],\n",
            path_e, method_lc, sum_e, name_e, tag_e);

        /* parameters */
        if (r->param_count > 0) {
            sb_append(&sb, "        \"parameters\": [\n");
            for (int j = 0; j < r->param_count; j++) {
                fl_api_param_t *pm = &r->params[j];
                char *pname_e = json_escape(pm->name);
                char *pdesc_e = json_escape(pm->desc);
                char *ptype_e = json_escape(pm->type[0] ? pm->type : "string");
                if (j > 0) sb_append(&sb, ",\n");
                sb_appendf(&sb,
                    "          {\n"
                    "            \"name\": \"%s\",\n"
                    "            \"in\": \"query\",\n"
                    "            \"required\": %s,\n"
                    "            \"description\": \"%s\",\n"
                    "            \"schema\": { \"type\": \"%s\" }\n"
                    "          }",
                    pname_e, pm->required ? "true" : "false", pdesc_e, ptype_e);
                free(pname_e); free(pdesc_e); free(ptype_e);
            }
            sb_append(&sb, "\n        ],\n");
        }

        /* responses */
        char *ret_e = json_escape(r->returns[0] ? r->returns : "object");
        sb_appendf(&sb,
            "        \"responses\": {\n"
            "          \"200\": {\n"
            "            \"description\": \"Success\",\n"
            "            \"content\": { \"application/json\": { \"schema\": { \"type\": \"%s\" } } }\n"
            "          }\n"
            "        }\n"
            "      }\n"
            "    }",
            ret_e);
        free(ret_e);
        free(path_e); free(sum_e); free(name_e); free(tag_e);
    }
    sb_append(&sb, "\n  }\n}\n");
    return sb.buf;
}

/* ============================================================
   라우트 목록 JSON (경량)
   ============================================================ */
char *fl_autodoc_routes_json(void) {
    strbuf_t sb;
    sb_init(&sb);
    sb_append(&sb, "[");
    for (int i = 0; i < fl_autodoc_global.count; i++) {
        fl_api_route_t *r = &fl_autodoc_global.routes[i];
        char *name_e = json_escape(r->name);
        char *path_e = json_escape(r->path);
        char *sum_e  = json_escape(r->summary);
        char *tag_e  = json_escape(r->tag[0] ? r->tag : "default");
        if (i > 0) sb_append(&sb, ",");
        sb_appendf(&sb,
            "{\"name\":\"%s\",\"path\":\"%s\",\"method\":\"%s\","
            "\"summary\":\"%s\",\"tag\":\"%s\",\"params\":%d}",
            name_e, path_e, r->method, sum_e, tag_e, r->param_count);
        free(name_e); free(path_e); free(sum_e); free(tag_e);
    }
    sb_append(&sb, "]");
    return sb.buf;
}

/* ============================================================
   내장 HTML UI (외부 CDN 0%)
   순수 HTML + CSS + Vanilla JS
   ============================================================ */
char *fl_autodoc_render_html(void) {
    /* OpenAPI JSON 먼저 생성 */
    char *openapi_json = fl_autodoc_to_openapi_json();

    strbuf_t sb;
    sb_init(&sb);

    char *title_e = json_escape(fl_autodoc_global.title);
    char *ver_e   = json_escape(fl_autodoc_global.version);

    sb_append(&sb,
        "<!DOCTYPE html>\n"
        "<html lang=\"ko\">\n"
        "<head>\n"
        "<meta charset=\"UTF-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n");
    sb_appendf(&sb, "<title>%s - MOSS-Autodoc</title>\n", fl_autodoc_global.title);
    sb_append(&sb,
        "<style>\n"
        "* { box-sizing: border-box; margin: 0; padding: 0; }\n"
        "body { font-family: 'Segoe UI', Arial, sans-serif; background: #0f1117; color: #e2e8f0; }\n"
        ".header { background: linear-gradient(135deg, #1a1f2e, #252d3d); padding: 24px 40px;"
        "  border-bottom: 1px solid #2d3748; display: flex; align-items: center; gap: 16px; }\n"
        ".header h1 { font-size: 1.5rem; color: #63b3ed; font-weight: 700; }\n"
        ".header .ver { background: #2d4a6e; color: #90cdf4; padding: 3px 10px;"
        "  border-radius: 12px; font-size: 0.8rem; }\n"
        ".header .badge { background: #276749; color: #9ae6b4; padding: 3px 10px;"
        "  border-radius: 12px; font-size: 0.75rem; }\n"
        ".container { max-width: 1100px; margin: 0 auto; padding: 32px 20px; }\n"
        ".search-box { width: 100%; padding: 10px 16px; background: #1a1f2e;"
        "  border: 1px solid #2d3748; border-radius: 8px; color: #e2e8f0;"
        "  font-size: 0.95rem; margin-bottom: 24px; outline: none; }\n"
        ".search-box:focus { border-color: #63b3ed; }\n"
        ".stats { display: flex; gap: 16px; margin-bottom: 24px; flex-wrap: wrap; }\n"
        ".stat { background: #1a1f2e; border: 1px solid #2d3748; border-radius: 8px;"
        "  padding: 12px 20px; }\n"
        ".stat .num { font-size: 1.8rem; font-weight: 700; color: #63b3ed; }\n"
        ".stat .lbl { font-size: 0.75rem; color: #718096; margin-top: 2px; }\n"
        ".routes { display: flex; flex-direction: column; gap: 12px; }\n"
        ".route { background: #1a1f2e; border: 1px solid #2d3748; border-radius: 10px;"
        "  overflow: hidden; transition: border-color 0.2s; }\n"
        ".route:hover { border-color: #4a5568; }\n"
        ".route-header { display: flex; align-items: center; gap: 12px; padding: 14px 20px;"
        "  cursor: pointer; user-select: none; }\n"
        ".method { padding: 4px 12px; border-radius: 6px; font-size: 0.78rem;"
        "  font-weight: 700; letter-spacing: 0.5px; min-width: 64px; text-align: center; }\n"
        ".GET    { background: #1a4731; color: #68d391; border: 1px solid #2f6a4e; }\n"
        ".POST   { background: #2a2360; color: #b794f4; border: 1px solid #4c3a8c; }\n"
        ".PUT    { background: #4a3000; color: #f6ad55; border: 1px solid #7b5213; }\n"
        ".DELETE { background: #4a1515; color: #fc8181; border: 1px solid #742a2a; }\n"
        ".PATCH  { background: #1a3a4a; color: #76e4f7; border: 1px solid #2c6a7e; }\n"
        ".path   { font-family: 'Courier New', monospace; color: #e2e8f0; font-size: 0.95rem; }\n"
        ".summary { color: #718096; font-size: 0.85rem; margin-left: auto; }\n"
        ".tag-pill { background: #2d3748; color: #90cdf4; padding: 2px 10px;"
        "  border-radius: 10px; font-size: 0.75rem; }\n"
        ".route-body { display: none; padding: 0 20px 16px 20px; border-top: 1px solid #2d3748; }\n"
        ".route-body.open { display: block; }\n"
        ".params-title { font-size: 0.8rem; color: #718096; margin: 12px 0 8px; text-transform: uppercase; letter-spacing: 1px; }\n"
        ".param-row { display: flex; gap: 12px; align-items: center;"
        "  padding: 8px 12px; background: #0f1117; border-radius: 6px; margin-bottom: 6px; }\n"
        ".param-name { font-family: monospace; color: #90cdf4; font-size: 0.9rem; min-width: 120px; }\n"
        ".param-type { color: #f6ad55; font-size: 0.8rem; min-width: 70px; }\n"
        ".param-req  { color: #fc8181; font-size: 0.75rem; }\n"
        ".param-desc { color: #a0aec0; font-size: 0.85rem; }\n"
        ".json-btn { margin-top: 16px; padding: 8px 16px; background: #2d4a6e;"
        "  color: #90cdf4; border: 1px solid #4a7aae; border-radius: 6px;"
        "  cursor: pointer; font-size: 0.85rem; }\n"
        ".json-btn:hover { background: #3a5a7e; }\n"
        ".json-modal { display: none; position: fixed; top: 0; left: 0; right: 0; bottom: 0;"
        "  background: rgba(0,0,0,0.8); z-index: 100; align-items: center; justify-content: center; }\n"
        ".json-modal.open { display: flex; }\n"
        ".json-box { background: #1a1f2e; border: 1px solid #4a5568; border-radius: 12px;"
        "  padding: 24px; max-width: 800px; width: 95%; max-height: 80vh; overflow: auto; }\n"
        ".json-box pre { color: #9ae6b4; font-size: 0.82rem; white-space: pre-wrap; word-break: break-all; }\n"
        ".close-btn { float: right; cursor: pointer; color: #718096; font-size: 1.2rem; }\n"
        ".footer { text-align: center; color: #4a5568; font-size: 0.78rem;"
        "  margin-top: 48px; padding: 24px; }\n"
        "</style>\n"
        "</head>\n"
        "<body>\n");

    sb_appendf(&sb,
        "<div class=\"header\">\n"
        "  <div>\n"
        "    <h1>%s</h1>\n"
        "  </div>\n"
        "  <span class=\"ver\">v%s</span>\n"
        "  <span class=\"badge\">MOSS-Autodoc</span>\n"
        "  <span class=\"badge\">Zero-Dependency</span>\n"
        "</div>\n",
        fl_autodoc_global.title, fl_autodoc_global.version);

    sb_append(&sb, "<div class=\"container\">\n");

    /* 통계 */
    int get_cnt = 0, post_cnt = 0, put_cnt = 0, del_cnt = 0, other_cnt = 0;
    for (int i = 0; i < fl_autodoc_global.count; i++) {
        const char *m = fl_autodoc_global.routes[i].method;
        if (strcmp(m, "GET") == 0)    get_cnt++;
        else if (strcmp(m, "POST") == 0)   post_cnt++;
        else if (strcmp(m, "PUT") == 0)    put_cnt++;
        else if (strcmp(m, "DELETE") == 0) del_cnt++;
        else other_cnt++;
    }
    sb_appendf(&sb,
        "<div class=\"stats\">\n"
        "  <div class=\"stat\"><div class=\"num\">%d</div><div class=\"lbl\">Total Routes</div></div>\n"
        "  <div class=\"stat\"><div class=\"num\" style=\"color:#68d391\">%d</div><div class=\"lbl\">GET</div></div>\n"
        "  <div class=\"stat\"><div class=\"num\" style=\"color:#b794f4\">%d</div><div class=\"lbl\">POST</div></div>\n"
        "  <div class=\"stat\"><div class=\"num\" style=\"color:#f6ad55\">%d</div><div class=\"lbl\">PUT</div></div>\n"
        "  <div class=\"stat\"><div class=\"num\" style=\"color:#fc8181\">%d</div><div class=\"lbl\">DELETE</div></div>\n"
        "</div>\n",
        fl_autodoc_global.count, get_cnt, post_cnt, put_cnt, del_cnt);

    /* 검색 박스 */
    sb_append(&sb,
        "<input class=\"search-box\" type=\"text\" id=\"search\" placeholder=\"Search routes...\" "
        "onkeyup=\"filterRoutes()\">\n"
        "<div class=\"routes\" id=\"routes\">\n");

    /* 라우트 카드 */
    for (int i = 0; i < fl_autodoc_global.count; i++) {
        fl_api_route_t *r = &fl_autodoc_global.routes[i];
        char *tag = r->tag[0] ? r->tag : "default";

        sb_appendf(&sb,
            "<div class=\"route\" data-path=\"%s\" data-method=\"%s\" data-tag=\"%s\">\n"
            "  <div class=\"route-header\" onclick=\"toggleRoute(%d)\">\n"
            "    <span class=\"method %s\">%s</span>\n"
            "    <span class=\"path\">%s</span>\n"
            "    <span class=\"summary\">%s</span>\n"
            "    <span class=\"tag-pill\">%s</span>\n"
            "  </div>\n"
            "  <div class=\"route-body\" id=\"body-%d\">\n",
            r->path, r->method, tag,
            i,
            r->method, r->method,
            r->path,
            r->summary,
            tag,
            i);

        /* 파라미터 */
        if (r->param_count > 0) {
            sb_append(&sb, "    <div class=\"params-title\">Parameters</div>\n");
            for (int j = 0; j < r->param_count; j++) {
                fl_api_param_t *pm = &r->params[j];
                sb_appendf(&sb,
                    "    <div class=\"param-row\">\n"
                    "      <span class=\"param-name\">%s</span>\n"
                    "      <span class=\"param-type\">%s</span>\n"
                    "      <span class=\"param-req\">%s</span>\n"
                    "      <span class=\"param-desc\">%s</span>\n"
                    "    </div>\n",
                    pm->name,
                    pm->type[0] ? pm->type : "string",
                    pm->required ? "required" : "optional",
                    pm->desc);
            }
        } else {
            sb_append(&sb, "    <div style=\"color:#4a5568;font-size:0.85rem;margin-top:8px;\">No parameters</div>\n");
        }

        /* 함수 이름 */
        sb_appendf(&sb,
            "    <div style=\"margin-top:10px;color:#4a5568;font-size:0.78rem;\">FreeLang function: <code style=\"color:#90cdf4\">%s</code></div>\n",
            r->name);

        sb_append(&sb, "  </div>\n</div>\n");
    }

    sb_append(&sb, "</div>\n");  /* routes */

    /* OpenAPI JSON 다운로드 버튼 */
    sb_append(&sb,
        "<br>\n"
        "<button class=\"json-btn\" onclick=\"document.getElementById('jsonModal').classList.add('open')\">View OpenAPI 3.0 JSON</button>\n"
        "<a href=\"/api/docs/openapi.json\" style=\"margin-left:12px;color:#63b3ed;font-size:0.85rem;\">Download JSON</a>\n");

    sb_append(&sb, "</div>\n");  /* container */

    /* JSON 모달 */
    sb_append(&sb,
        "<div class=\"json-modal\" id=\"jsonModal\" onclick=\"if(event.target===this)this.classList.remove('open')\">\n"
        "  <div class=\"json-box\">\n"
        "    <span class=\"close-btn\" onclick=\"document.getElementById('jsonModal').classList.remove('open')\">X</span>\n"
        "    <h3 style=\"color:#63b3ed;margin-bottom:12px;\">OpenAPI 3.0 Specification</h3>\n"
        "    <pre id=\"jsonContent\"></pre>\n"
        "  </div>\n"
        "</div>\n");

    /* Footer */
    sb_appendf(&sb,
        "<div class=\"footer\">"
        "Powered by FreeLang MOSS-Autodoc | %s v%s | Zero external dependencies"
        "</div>\n",
        fl_autodoc_global.title, fl_autodoc_global.version);

    /* JavaScript */
    sb_append(&sb,
        "<script>\n"
        "function toggleRoute(i) {\n"
        "  var b = document.getElementById('body-' + i);\n"
        "  b.classList.toggle('open');\n"
        "}\n"
        "function filterRoutes() {\n"
        "  var q = document.getElementById('search').value.toLowerCase();\n"
        "  var routes = document.querySelectorAll('.route');\n"
        "  routes.forEach(function(r) {\n"
        "    var txt = (r.dataset.path + ' ' + r.dataset.method + ' ' + r.dataset.tag).toLowerCase();\n"
        "    r.style.display = txt.indexOf(q) >= 0 ? '' : 'none';\n"
        "  });\n"
        "}\n"
        "// Load OpenAPI JSON into modal\n"
        "document.getElementById('jsonModal').addEventListener('click', function(e) {\n"
        "  if (e.target === this) this.classList.remove('open');\n"
        "});\n"
        "document.querySelector('.json-btn').addEventListener('click', function() {\n"
        "  fetch('/api/docs/openapi.json').then(r=>r.json())"
        ".then(d=>{ document.getElementById('jsonContent').textContent = JSON.stringify(d,null,2); })"
        ".catch(()=>{ document.getElementById('jsonContent').textContent = 'Could not load JSON'; });\n"
        "});\n"
        "</script>\n"
        "</body>\n</html>\n");

    free(title_e); free(ver_e);
    if (openapi_json) free(openapi_json);
    return sb.buf;
}
