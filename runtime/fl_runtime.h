#ifndef FL_RUNTIME_H
#define FL_RUNTIME_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>

/* ============================================================================
   FreeLang Runtime - C Header

   모든 FreeLang 값을 동적 태그드 유니온으로 표현합니다.
   Phase 1에서 파싱한 AST를 C 코드로 변환하여 실행합니다.
   ============================================================================ */

/* 값 태그 */
typedef enum {
  FL_NULL,
  FL_BOOL,
  FL_INT,
  FL_FLOAT,
  FL_STRING,
  FL_ARRAY,
  FL_MAP,
  FL_CLOSURE,
  FL_ENUM,
  FL_STRUCT,
} fl_tag;

/* 전방 선언 */
typedef struct fl_value fl_value;
typedef struct fl_array fl_array;
typedef struct fl_map fl_map;
typedef struct fl_closure fl_closure;
typedef struct fl_enum_val fl_enum_val;
typedef struct fl_struct_val fl_struct_val;

/* 메인 값 타입 */
struct fl_value {
  fl_tag tag;
  union {
    int64_t ival;           /* FL_INT */
    double fval;            /* FL_FLOAT */
    int bval;               /* FL_BOOL (0 또는 1) */
    char *sval;             /* FL_STRING */
    fl_array *arrval;       /* FL_ARRAY */
    fl_map *mapval;         /* FL_MAP */
    fl_closure *fnval;      /* FL_CLOSURE */
    fl_enum_val *enumval;   /* FL_ENUM */
    fl_struct_val *structval; /* FL_STRUCT */
  };
};

/* 배열 */
struct fl_array {
  fl_value **data;
  size_t length;
  size_t capacity;
};

/* 맵 (단순 해시테이블) */
typedef struct fl_map_entry {
  char *key;
  fl_value *val;
  struct fl_map_entry *next;
} fl_map_entry;

struct fl_map {
  fl_map_entry **buckets;
  size_t bucket_count;
  size_t size;
};

/* 클로저 */
typedef fl_value* (*fl_fn_ptr)(fl_value **locals, int local_count);

struct fl_closure {
  fl_fn_ptr fn;
  fl_value **env;      /* 캡처된 변수 */
  int env_count;
};

/* Enum 값 */
struct fl_enum_val {
  const char *enum_name;
  const char *variant;
  fl_value **data;     /* 각 필드의 값 */
  int data_count;
};

/* Struct 값 */
struct fl_struct_val {
  const char *struct_name;
  fl_map *fields;      /* 필드명 → 값 */
};

/* ============================================================================
   함수 선언
   ============================================================================ */

/* 값 생성 */
fl_value* fl_null(void);
fl_value* fl_bool(int v);
fl_value* fl_int(int64_t v);
fl_value* fl_float(double v);
fl_value* fl_string(const char *s);
fl_value* fl_array_new(void);
fl_value* fl_map_new(void);
fl_value* fl_enum_new(const char *enum_name, const char *variant, fl_value **data, int data_count);
fl_value* fl_struct_new(const char *struct_name);

/* 배열 연산 */
void fl_array_push(fl_value *arr, fl_value *v);
fl_value* fl_array_get(fl_value *arr, int64_t idx);
void fl_array_set(fl_value *arr, int64_t idx, fl_value *v);
int64_t fl_array_len(fl_value *arr);

/* 맵 연산 */
void fl_map_set(fl_value *map, const char *key, fl_value *v);
fl_value* fl_map_get(fl_value *map, const char *key);
int fl_map_has(fl_value *map, const char *key);

/* Struct 연산 */
void fl_struct_set_field(fl_value *s, const char *field, fl_value *v);
fl_value* fl_struct_get_field(fl_value *s, const char *field);

/* 이항 연산 */
fl_value* fl_add(fl_value *a, fl_value *b);
fl_value* fl_sub(fl_value *a, fl_value *b);
fl_value* fl_mul(fl_value *a, fl_value *b);
fl_value* fl_div(fl_value *a, fl_value *b);
fl_value* fl_mod(fl_value *a, fl_value *b);

/* 비교 연산 */
fl_value* fl_eq(fl_value *a, fl_value *b);
fl_value* fl_neq(fl_value *a, fl_value *b);
fl_value* fl_lt(fl_value *a, fl_value *b);
fl_value* fl_gt(fl_value *a, fl_value *b);
fl_value* fl_lte(fl_value *a, fl_value *b);
fl_value* fl_gte(fl_value *a, fl_value *b);

/* 논리 연산 */
fl_value* fl_and(fl_value *a, fl_value *b);
fl_value* fl_or(fl_value *a, fl_value *b);
fl_value* fl_not(fl_value *a);

/* 타입 변환 */
int fl_is_truthy(fl_value *v);
int64_t fl_to_int(fl_value *v);
double fl_to_float(fl_value *v);
char* fl_to_string(fl_value *v);

/* I/O */
void fl_print(fl_value *v);
void fl_println(fl_value *v);

/* 메모리 관리 */
void fl_gc_run(void);
void fl_cleanup(void);

/* 예외 처리 */
extern jmp_buf fl_exception_buf;
extern fl_value *fl_last_exception;

void fl_throw(fl_value *err);

#define FL_TRY if (setjmp(fl_exception_buf) == 0)
#define FL_CATCH else

#endif /* FL_RUNTIME_H */
