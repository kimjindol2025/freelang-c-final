#include "fl_runtime.h"

/* ============================================================================
   글로벌 상태
   ============================================================================ */

jmp_buf fl_exception_buf;
fl_value *fl_last_exception = NULL;

/* 간단한 메모리 할당 (실제로는 GC 필요) */
static void* fl_malloc(size_t size) {
  void *ptr = malloc(size);
  if (!ptr) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(1);
  }
  return ptr;
}

/* ============================================================================
   값 생성 함수들
   ============================================================================ */

fl_value* fl_null(void) {
  fl_value *v = fl_malloc(sizeof(fl_value));
  v->tag = FL_NULL;
  return v;
}

fl_value* fl_bool(int v) {
  fl_value *val = fl_malloc(sizeof(fl_value));
  val->tag = FL_BOOL;
  val->bval = v ? 1 : 0;
  return val;
}

fl_value* fl_int(int64_t v) {
  fl_value *val = fl_malloc(sizeof(fl_value));
  val->tag = FL_INT;
  val->ival = v;
  return val;
}

fl_value* fl_float(double v) {
  fl_value *val = fl_malloc(sizeof(fl_value));
  val->tag = FL_FLOAT;
  val->fval = v;
  return val;
}

fl_value* fl_string(const char *s) {
  fl_value *val = fl_malloc(sizeof(fl_value));
  val->tag = FL_STRING;
  val->sval = fl_malloc(strlen(s) + 1);
  strcpy(val->sval, s);
  return val;
}

fl_value* fl_array_new(void) {
  fl_value *val = fl_malloc(sizeof(fl_value));
  val->tag = FL_ARRAY;
  val->arrval = fl_malloc(sizeof(fl_array));
  val->arrval->data = fl_malloc(sizeof(fl_value*) * 10);
  val->arrval->length = 0;
  val->arrval->capacity = 10;
  return val;
}

void fl_array_push(fl_value *arr, fl_value *v) {
  if (arr->tag != FL_ARRAY) return;
  fl_array *a = arr->arrval;
  if (a->length >= a->capacity) {
    a->capacity *= 2;
    a->data = realloc(a->data, sizeof(fl_value*) * a->capacity);
  }
  a->data[a->length++] = v;
}

fl_value* fl_array_get(fl_value *arr, int64_t idx) {
  if (arr->tag != FL_ARRAY) return fl_null();
  fl_array *a = arr->arrval;
  if (idx < 0 || idx >= (int64_t)a->length) return fl_null();
  return a->data[idx];
}

void fl_array_set(fl_value *arr, int64_t idx, fl_value *v) {
  if (arr->tag != FL_ARRAY) return;
  fl_array *a = arr->arrval;
  if (idx < 0 || idx >= (int64_t)a->length) return;
  a->data[idx] = v;
}

int64_t fl_array_len(fl_value *arr) {
  if (arr->tag != FL_ARRAY) return 0;
  return arr->arrval->length;
}

fl_value* fl_map_new(void) {
  fl_value *val = fl_malloc(sizeof(fl_value));
  val->tag = FL_MAP;
  val->mapval = fl_malloc(sizeof(fl_map));
  val->mapval->bucket_count = 16;
  val->mapval->buckets = fl_malloc(sizeof(fl_map_entry*) * 16);
  memset(val->mapval->buckets, 0, sizeof(fl_map_entry*) * 16);
  val->mapval->size = 0;
  return val;
}

static size_t fl_hash(const char *key) {
  size_t hash = 5381;
  while (*key) hash = ((hash << 5) + hash) + *key++;
  return hash;
}

void fl_map_set(fl_value *map, const char *key, fl_value *v) {
  if (map->tag != FL_MAP) return;
  fl_map *m = map->mapval;
  size_t idx = fl_hash(key) % m->bucket_count;
  fl_map_entry *e = m->buckets[idx];
  while (e) {
    if (strcmp(e->key, key) == 0) {
      e->val = v;
      return;
    }
    e = e->next;
  }
  fl_map_entry *ne = fl_malloc(sizeof(fl_map_entry));
  ne->key = fl_malloc(strlen(key) + 1);
  strcpy(ne->key, key);
  ne->val = v;
  ne->next = m->buckets[idx];
  m->buckets[idx] = ne;
  m->size++;
}

fl_value* fl_map_get(fl_value *map, const char *key) {
  if (map->tag != FL_MAP) return fl_null();
  fl_map *m = map->mapval;
  size_t idx = fl_hash(key) % m->bucket_count;
  fl_map_entry *e = m->buckets[idx];
  while (e) {
    if (strcmp(e->key, key) == 0) return e->val;
    e = e->next;
  }
  return fl_null();
}

int fl_map_has(fl_value *map, const char *key) {
  if (map->tag != FL_MAP) return 0;
  fl_map *m = map->mapval;
  size_t idx = fl_hash(key) % m->bucket_count;
  fl_map_entry *e = m->buckets[idx];
  while (e) {
    if (strcmp(e->key, key) == 0) return 1;
    e = e->next;
  }
  return 0;
}

fl_value* fl_enum_new(const char *enum_name, const char *variant, fl_value **data, int data_count) {
  fl_value *val = fl_malloc(sizeof(fl_value));
  val->tag = FL_ENUM;
  val->enumval = fl_malloc(sizeof(fl_enum_val));
  val->enumval->enum_name = enum_name;
  val->enumval->variant = variant;
  val->enumval->data = data;
  val->enumval->data_count = data_count;
  return val;
}

fl_value* fl_struct_new(const char *struct_name) {
  fl_value *val = fl_malloc(sizeof(fl_value));
  val->tag = FL_STRUCT;
  val->structval = fl_malloc(sizeof(fl_struct_val));
  val->structval->struct_name = struct_name;
  val->structval->fields = fl_map_new()->mapval;
  return val;
}

void fl_struct_set_field(fl_value *s, const char *field, fl_value *v) {
  if (s->tag != FL_STRUCT) return;
  fl_value temp_val;
  temp_val.tag = FL_MAP;
  temp_val.mapval = s->structval->fields;
  fl_map_set(&temp_val, field, v);
}

fl_value* fl_struct_get_field(fl_value *s, const char *field) {
  if (s->tag != FL_STRUCT) return fl_null();
  fl_value temp_val;
  temp_val.tag = FL_MAP;
  temp_val.mapval = s->structval->fields;
  return fl_map_get(&temp_val, field);
}

/* ============================================================================
   이항 연산자들
   ============================================================================ */

fl_value* fl_add(fl_value *a, fl_value *b) {
  if (a->tag == FL_INT && b->tag == FL_INT) {
    return fl_int(a->ival + b->ival);
  }
  if ((a->tag == FL_INT || a->tag == FL_FLOAT) &&
      (b->tag == FL_INT || b->tag == FL_FLOAT)) {
    double af = (a->tag == FL_INT) ? (double)a->ival : a->fval;
    double bf = (b->tag == FL_INT) ? (double)b->ival : b->fval;
    return fl_float(af + bf);
  }
  if (a->tag == FL_STRING && b->tag == FL_STRING) {
    char *result = fl_malloc(strlen(a->sval) + strlen(b->sval) + 1);
    strcpy(result, a->sval);
    strcat(result, b->sval);
    fl_value *v = fl_malloc(sizeof(fl_value));
    v->tag = FL_STRING;
    v->sval = result;
    return v;
  }
  return fl_null();
}

fl_value* fl_sub(fl_value *a, fl_value *b) {
  if (a->tag == FL_INT && b->tag == FL_INT) {
    return fl_int(a->ival - b->ival);
  }
  if ((a->tag == FL_INT || a->tag == FL_FLOAT) &&
      (b->tag == FL_INT || b->tag == FL_FLOAT)) {
    double af = (a->tag == FL_INT) ? (double)a->ival : a->fval;
    double bf = (b->tag == FL_INT) ? (double)b->ival : b->fval;
    return fl_float(af - bf);
  }
  return fl_null();
}

fl_value* fl_mul(fl_value *a, fl_value *b) {
  if (a->tag == FL_INT && b->tag == FL_INT) {
    return fl_int(a->ival * b->ival);
  }
  if ((a->tag == FL_INT || a->tag == FL_FLOAT) &&
      (b->tag == FL_INT || b->tag == FL_FLOAT)) {
    double af = (a->tag == FL_INT) ? (double)a->ival : a->fval;
    double bf = (b->tag == FL_INT) ? (double)b->ival : b->fval;
    return fl_float(af * bf);
  }
  return fl_null();
}

fl_value* fl_div(fl_value *a, fl_value *b) {
  if (a->tag == FL_INT && b->tag == FL_INT && b->ival != 0) {
    return fl_int(a->ival / b->ival);
  }
  if ((a->tag == FL_INT || a->tag == FL_FLOAT) &&
      (b->tag == FL_INT || b->tag == FL_FLOAT)) {
    double af = (a->tag == FL_INT) ? (double)a->ival : a->fval;
    double bf = (b->tag == FL_INT) ? (double)b->ival : b->fval;
    if (bf != 0) return fl_float(af / bf);
  }
  return fl_null();
}

fl_value* fl_mod(fl_value *a, fl_value *b) {
  if (a->tag == FL_INT && b->tag == FL_INT && b->ival != 0) {
    return fl_int(a->ival % b->ival);
  }
  return fl_null();
}

/* ============================================================================
   비교 연산자들
   ============================================================================ */

fl_value* fl_eq(fl_value *a, fl_value *b) {
  if (a->tag != b->tag) return fl_bool(0);
  switch (a->tag) {
    case FL_NULL: return fl_bool(1);
    case FL_BOOL: return fl_bool(a->bval == b->bval);
    case FL_INT: return fl_bool(a->ival == b->ival);
    case FL_FLOAT: return fl_bool(a->fval == b->fval);
    case FL_STRING: return fl_bool(strcmp(a->sval, b->sval) == 0);
    default: return fl_bool(0);
  }
}

fl_value* fl_neq(fl_value *a, fl_value *b) {
  fl_value *eq = fl_eq(a, b);
  int result = !eq->bval;
  free(eq);
  return fl_bool(result);
}

fl_value* fl_lt(fl_value *a, fl_value *b) {
  if (a->tag == FL_INT && b->tag == FL_INT) return fl_bool(a->ival < b->ival);
  if ((a->tag == FL_INT || a->tag == FL_FLOAT) &&
      (b->tag == FL_INT || b->tag == FL_FLOAT)) {
    double af = (a->tag == FL_INT) ? (double)a->ival : a->fval;
    double bf = (b->tag == FL_INT) ? (double)b->ival : b->fval;
    return fl_bool(af < bf);
  }
  return fl_bool(0);
}

fl_value* fl_gt(fl_value *a, fl_value *b) {
  if (a->tag == FL_INT && b->tag == FL_INT) return fl_bool(a->ival > b->ival);
  if ((a->tag == FL_INT || a->tag == FL_FLOAT) &&
      (b->tag == FL_INT || b->tag == FL_FLOAT)) {
    double af = (a->tag == FL_INT) ? (double)a->ival : a->fval;
    double bf = (b->tag == FL_INT) ? (double)b->ival : b->fval;
    return fl_bool(af > bf);
  }
  return fl_bool(0);
}

fl_value* fl_lte(fl_value *a, fl_value *b) {
  fl_value *gt = fl_gt(a, b);
  int result = !gt->bval;
  free(gt);
  return fl_bool(result);
}

fl_value* fl_gte(fl_value *a, fl_value *b) {
  fl_value *lt = fl_lt(a, b);
  int result = !lt->bval;
  free(lt);
  return fl_bool(result);
}

/* ============================================================================
   논리 연산자들
   ============================================================================ */

fl_value* fl_and(fl_value *a, fl_value *b) {
  return fl_bool(fl_is_truthy(a) && fl_is_truthy(b));
}

fl_value* fl_or(fl_value *a, fl_value *b) {
  return fl_bool(fl_is_truthy(a) || fl_is_truthy(b));
}

fl_value* fl_not(fl_value *a) {
  return fl_bool(!fl_is_truthy(a));
}

/* ============================================================================
   타입 변환
   ============================================================================ */

int fl_is_truthy(fl_value *v) {
  if (v->tag == FL_NULL) return 0;
  if (v->tag == FL_BOOL) return v->bval;
  if (v->tag == FL_INT) return v->ival != 0;
  if (v->tag == FL_FLOAT) return v->fval != 0.0;
  return 1;
}

int64_t fl_to_int(fl_value *v) {
  switch (v->tag) {
    case FL_INT: return v->ival;
    case FL_FLOAT: return (int64_t)v->fval;
    case FL_BOOL: return v->bval ? 1 : 0;
    default: return 0;
  }
}

double fl_to_float(fl_value *v) {
  switch (v->tag) {
    case FL_INT: return (double)v->ival;
    case FL_FLOAT: return v->fval;
    case FL_BOOL: return v->bval ? 1.0 : 0.0;
    default: return 0.0;
  }
}

char* fl_to_string(fl_value *v) {
  static char buf[256];
  switch (v->tag) {
    case FL_NULL:
      strcpy(buf, "null");
      break;
    case FL_BOOL:
      strcpy(buf, v->bval ? "true" : "false");
      break;
    case FL_INT:
      snprintf(buf, sizeof(buf), "%ld", v->ival);
      break;
    case FL_FLOAT:
      snprintf(buf, sizeof(buf), "%f", v->fval);
      break;
    case FL_STRING:
      return v->sval;
    default:
      strcpy(buf, "[Object]");
  }
  return buf;
}

/* ============================================================================
   I/O
   ============================================================================ */

void fl_print(fl_value *v) {
  printf("%s", fl_to_string(v));
}

void fl_println(fl_value *v) {
  printf("%s\n", fl_to_string(v));
}

/* ============================================================================
   예외 처리
   ============================================================================ */

void fl_throw(fl_value *err) {
  fl_last_exception = err;
  longjmp(fl_exception_buf, 1);
}

/* ============================================================================
   메모리 관리 (간단한 구현)
   ============================================================================ */

void fl_gc_run(void) {
  /* 실제 GC는 Phase 3에서 구현 */
}

void fl_cleanup(void) {
  /* 정리 작업 */
}
