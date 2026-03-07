/**
 * FreeLang Standard Library Implementation
 * Complete builtin functions: I/O, String, Array, Math, Type conversion, JSON, etc.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <stdint.h>
#include "../include/stdlib_fl.h"
#include "../include/runtime.h"
#include "../include/compression.h"

/* Helper functions */
static fl_value_t fl_new_null() {
    fl_value_t result;
    result.type = FL_TYPE_NULL;
    return result;
}

static fl_value_t fl_new_int(fl_int val) {
    fl_value_t result;
    result.type = FL_TYPE_INT;
    result.data.int_val = val;
    return result;
}

static fl_value_t fl_new_float(fl_float val) {
    fl_value_t result;
    result.type = FL_TYPE_FLOAT;
    result.data.float_val = val;
    return result;
}

static fl_value_t fl_new_bool(fl_bool val) {
    fl_value_t result;
    result.type = FL_TYPE_BOOL;
    result.data.bool_val = val;
    return result;
}

static fl_value_t fl_new_string(const char* str) {
    fl_value_t result;
    result.type = FL_TYPE_STRING;
    if (str) {
        result.data.string_val = malloc(strlen(str) + 1);
        strcpy(result.data.string_val, str);
    } else {
        result.data.string_val = NULL;
    }
    return result;
}

static fl_array_t* fl_array_create(size_t capacity) {
    fl_array_t* arr = malloc(sizeof(fl_array_t));
    arr->capacity = capacity > 0 ? capacity : 10;
    arr->size = 0;
    arr->elements = malloc(arr->capacity * sizeof(fl_value_t));
    return arr;
}

static fl_value_t fl_new_array(size_t capacity) {
    fl_value_t result;
    result.type = FL_TYPE_ARRAY;
    result.data.array_val = fl_array_create(capacity);
    return result;
}

static fl_object_t* fl_object_create(size_t capacity) {
    fl_object_t* obj = malloc(sizeof(fl_object_t));
    obj->capacity = capacity > 0 ? capacity : 10;
    obj->size = 0;
    obj->keys = malloc(obj->capacity * sizeof(char*));
    obj->values = malloc(obj->capacity * sizeof(fl_value_t));
    return obj;
}

static fl_value_t fl_new_object(void) {
    fl_value_t result;
    result.type = FL_TYPE_OBJECT;
    result.data.object_val = fl_object_create(10);
    return result;
}

/* Bytes helper functions */
static fl_bytes_t* fl_bytes_create(size_t capacity) {
    fl_bytes_t* b = malloc(sizeof(fl_bytes_t));
    b->capacity = capacity > 0 ? capacity : 256;
    b->size = 0;
    b->data = malloc(b->capacity);
    return b;
}

static fl_value_t fl_new_bytes(size_t capacity) {
    fl_value_t result;
    result.type = FL_TYPE_BYTES;
    result.data.bytes_val = fl_bytes_create(capacity);
    return result;
}

static void fl_bytes_expand(fl_bytes_t* b, size_t needed_size) {
    if (needed_size <= b->capacity) return;
    size_t new_capacity = b->capacity * 2;
    while (new_capacity < needed_size) new_capacity *= 2;
    b->data = realloc(b->data, new_capacity);
    b->capacity = new_capacity;
}

/* I/O Functions */

fl_value_t fl_print(fl_value_t* args, size_t argc) {
    for (size_t i = 0; i < argc; i++) {
        fl_value_print(args[i]);
    }
    return fl_new_null();
}

fl_value_t fl_println(fl_value_t* args, size_t argc) {
    for (size_t i = 0; i < argc; i++) {
        fl_value_print(args[i]);
        if (i < argc - 1) printf(" ");
    }
    printf("\n");
    return fl_new_null();
}

fl_value_t fl_input(fl_value_t* args, size_t argc) {
    /* Print prompt if provided */
    if (argc > 0 && args[0].type == FL_TYPE_STRING) {
        printf("%s", args[0].data.string_val);
        fflush(stdout);
    }

    /* Read line from stdin */
    char buffer[4096];
    if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        /* Remove trailing newline */
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
        return fl_new_string(buffer);
    }

    return fl_new_null();
}

/* String Functions */

fl_value_t fl_string_length(fl_value_t* args, size_t argc) {
    if (argc != 1 || args[0].type != FL_TYPE_STRING) {
        return fl_new_null();
    }

    return fl_new_int((fl_int)strlen(args[0].data.string_val));
}

fl_value_t fl_string_concat(fl_value_t* args, size_t argc) {
    if (argc < 2) return fl_new_null();

    /* Calculate total length needed */
    size_t total_len = 0;
    for (size_t i = 0; i < argc; i++) {
        if (args[i].type == FL_TYPE_STRING) {
            total_len += strlen(args[i].data.string_val);
        }
    }

    /* Allocate and concatenate */
    char* result = malloc(total_len + 1);
    result[0] = '\0';

    for (size_t i = 0; i < argc; i++) {
        if (args[i].type == FL_TYPE_STRING) {
            strcat(result, args[i].data.string_val);
        }
    }

    fl_value_t ret;
    ret.type = FL_TYPE_STRING;
    ret.data.string_val = result;
    return ret;
}

fl_value_t fl_string_slice(fl_value_t* args, size_t argc) {
    if (argc != 3 || args[0].type != FL_TYPE_STRING) {
        return fl_new_null();
    }

    const char* str = args[0].data.string_val;
    fl_int start = (args[1].type == FL_TYPE_INT) ? args[1].data.int_val : 0;
    fl_int end = (args[2].type == FL_TYPE_INT) ? args[2].data.int_val : (fl_int)strlen(str);

    size_t len = strlen(str);
    if (start < 0) start = 0;
    if (end > (fl_int)len) end = (fl_int)len;
    if (start > end) start = end;

    size_t slice_len = (size_t)(end - start);
    char* slice = malloc(slice_len + 1);
    strncpy(slice, str + start, slice_len);
    slice[slice_len] = '\0';

    fl_value_t ret;
    ret.type = FL_TYPE_STRING;
    ret.data.string_val = slice;
    return ret;
}

fl_value_t fl_string_split(fl_value_t* args, size_t argc) {
    if (argc < 2 || args[0].type != FL_TYPE_STRING || args[1].type != FL_TYPE_STRING) {
        return fl_new_null();
    }

    const char* str = args[0].data.string_val;
    const char* delim = args[1].data.string_val;

    /* Count occurrences */
    char* copy = malloc(strlen(str) + 1);
    strcpy(copy, str);

    fl_array_t* arr = fl_array_create(10);
    char* token = strtok(copy, delim);

    while (token != NULL) {
        fl_value_t elem = fl_new_string(token);
        if (arr->size >= arr->capacity) {
            arr->capacity *= 2;
            arr->elements = realloc(arr->elements, arr->capacity * sizeof(fl_value_t));
        }
        arr->elements[arr->size++] = elem;
        token = strtok(NULL, delim);
    }

    free(copy);

    fl_value_t ret;
    ret.type = FL_TYPE_ARRAY;
    ret.data.array_val = arr;
    return ret;
}

fl_value_t fl_string_trim(fl_value_t* args, size_t argc) {
    if (argc != 1 || args[0].type != FL_TYPE_STRING) {
        return fl_new_null();
    }

    const char* str = args[0].data.string_val;
    size_t len = strlen(str);

    /* Find start (skip leading whitespace) */
    size_t start = 0;
    while (start < len && isspace((unsigned char)str[start])) {
        start++;
    }

    /* Find end (skip trailing whitespace) */
    size_t end = len;
    while (end > start && isspace((unsigned char)str[end - 1])) {
        end--;
    }

    size_t trim_len = end - start;
    char* trimmed = malloc(trim_len + 1);
    strncpy(trimmed, str + start, trim_len);
    trimmed[trim_len] = '\0';

    return fl_new_string(trimmed);
}

fl_value_t fl_string_upper(fl_value_t* args, size_t argc) {
    if (argc != 1 || args[0].type != FL_TYPE_STRING) {
        return fl_new_null();
    }

    const char* str = args[0].data.string_val;
    size_t len = strlen(str);
    char* upper = malloc(len + 1);

    for (size_t i = 0; i < len; i++) {
        upper[i] = (char)toupper((unsigned char)str[i]);
    }
    upper[len] = '\0';

    return fl_new_string(upper);
}

fl_value_t fl_string_lower(fl_value_t* args, size_t argc) {
    if (argc != 1 || args[0].type != FL_TYPE_STRING) {
        return fl_new_null();
    }

    const char* str = args[0].data.string_val;
    size_t len = strlen(str);
    char* lower = malloc(len + 1);

    for (size_t i = 0; i < len; i++) {
        lower[i] = (char)tolower((unsigned char)str[i]);
    }
    lower[len] = '\0';

    return fl_new_string(lower);
}

fl_value_t fl_string_contains(fl_value_t* args, size_t argc) {
    if (argc != 2 || args[0].type != FL_TYPE_STRING || args[1].type != FL_TYPE_STRING) {
        return fl_new_null();
    }

    const char* str = args[0].data.string_val;
    const char* substr = args[1].data.string_val;

    return fl_new_bool(strstr(str, substr) != NULL);
}

fl_value_t fl_string_starts_with(fl_value_t* args, size_t argc) {
    if (argc != 2 || args[0].type != FL_TYPE_STRING || args[1].type != FL_TYPE_STRING) {
        return fl_new_null();
    }

    const char* str = args[0].data.string_val;
    const char* prefix = args[1].data.string_val;

    return fl_new_bool(strncmp(str, prefix, strlen(prefix)) == 0);
}

fl_value_t fl_string_ends_with(fl_value_t* args, size_t argc) {
    if (argc != 2 || args[0].type != FL_TYPE_STRING || args[1].type != FL_TYPE_STRING) {
        return fl_new_null();
    }

    const char* str = args[0].data.string_val;
    const char* suffix = args[1].data.string_val;

    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len) return fl_new_bool(false);

    return fl_new_bool(strcmp(str + str_len - suffix_len, suffix) == 0);
}

fl_value_t fl_string_replace(fl_value_t* args, size_t argc) {
    if (argc != 3 || args[0].type != FL_TYPE_STRING ||
        args[1].type != FL_TYPE_STRING || args[2].type != FL_TYPE_STRING) {
        return fl_new_null();
    }

    const char* str = args[0].data.string_val;
    const char* find = args[1].data.string_val;
    const char* replace = args[2].data.string_val;

    /* Simple replace: find first occurrence and replace */
    const char* pos = strstr(str, find);
    if (pos == NULL) {
        return fl_new_string(str);
    }

    size_t before_len = pos - str;
    size_t result_len = before_len + strlen(replace) + strlen(pos + strlen(find)) + 1;
    char* result = malloc(result_len);

    strncpy(result, str, before_len);
    strcpy(result + before_len, replace);
    strcpy(result + before_len + strlen(replace), pos + strlen(find));

    return fl_new_string(result);
}

fl_value_t fl_string_index_of(fl_value_t* args, size_t argc) {
    if (argc != 2 || args[0].type != FL_TYPE_STRING || args[1].type != FL_TYPE_STRING) {
        return fl_new_null();
    }

    const char* str = args[0].data.string_val;
    const char* substr = args[1].data.string_val;

    const char* pos = strstr(str, substr);
    if (pos == NULL) {
        return fl_new_int(-1);
    }

    return fl_new_int((fl_int)(pos - str));
}

fl_value_t fl_string_repeat(fl_value_t* args, size_t argc) {
    if (argc != 2 || args[0].type != FL_TYPE_STRING) {
        return fl_new_null();
    }

    const char* str = args[0].data.string_val;
    fl_int count = (args[1].type == FL_TYPE_INT) ? args[1].data.int_val : 0;

    if (count <= 0) {
        return fl_new_string("");
    }

    size_t str_len = strlen(str);
    size_t result_len = str_len * (size_t)count + 1;
    char* result = malloc(result_len);
    result[0] = '\0';

    for (fl_int i = 0; i < count; i++) {
        strcat(result, str);
    }

    return fl_new_string(result);
}

fl_value_t fl_string_pad_start(fl_value_t* args, size_t argc) {
    if (argc != 3 || args[0].type != FL_TYPE_STRING || args[2].type != FL_TYPE_STRING) {
        return fl_new_null();
    }

    const char* str = args[0].data.string_val;
    fl_int target_len = (args[1].type == FL_TYPE_INT) ? args[1].data.int_val : 0;
    const char* pad_str = args[2].data.string_val;

    size_t str_len = strlen(str);
    if (target_len <= (fl_int)str_len) {
        return fl_new_string(str);
    }

    size_t pad_needed = (size_t)target_len - str_len;
    size_t pad_len = strlen(pad_str);
    size_t result_len = (size_t)target_len + 1;
    char* result = malloc(result_len);
    result[0] = '\0';

    /* Add padding */
    for (size_t i = 0; i < pad_needed; i++) {
        result[i] = pad_str[i % pad_len];
    }
    strcpy(result + pad_needed, str);

    return fl_new_string(result);
}

fl_value_t fl_string_pad_end(fl_value_t* args, size_t argc) {
    if (argc != 3 || args[0].type != FL_TYPE_STRING || args[2].type != FL_TYPE_STRING) {
        return fl_new_null();
    }

    const char* str = args[0].data.string_val;
    fl_int target_len = (args[1].type == FL_TYPE_INT) ? args[1].data.int_val : 0;
    const char* pad_str = args[2].data.string_val;

    size_t str_len = strlen(str);
    if (target_len <= (fl_int)str_len) {
        return fl_new_string(str);
    }

    size_t pad_needed = (size_t)target_len - str_len;
    size_t pad_len = strlen(pad_str);
    size_t result_len = (size_t)target_len + 1;
    char* result = malloc(result_len);

    strcpy(result, str);

    /* Add padding */
    for (size_t i = 0; i < pad_needed; i++) {
        result[str_len + i] = pad_str[i % pad_len];
    }
    result[(size_t)target_len] = '\0';

    return fl_new_string(result);
}

fl_value_t fl_string_char_at(fl_value_t* args, size_t argc) {
    if (argc != 2 || args[0].type != FL_TYPE_STRING) {
        return fl_new_null();
    }

    const char* str = args[0].data.string_val;
    fl_int idx = (args[1].type == FL_TYPE_INT) ? args[1].data.int_val : 0;

    if (idx < 0 || idx >= (fl_int)strlen(str)) {
        return fl_new_null();
    }

    char ch[2] = {str[idx], '\0'};
    return fl_new_string(ch);
}

/* Array Functions */

fl_value_t fl_array_length(fl_value_t* args, size_t argc) {
    if (argc != 1 || args[0].type != FL_TYPE_ARRAY) {
        return fl_new_null();
    }

    return fl_new_int((fl_int)args[0].data.array_val->size);
}

fl_value_t fl_array_push(fl_value_t* args, size_t argc) {
    if (argc != 2 || args[0].type != FL_TYPE_ARRAY) {
        return fl_new_null();
    }

    fl_array_t* arr = args[0].data.array_val;

    if (arr->size >= arr->capacity) {
        arr->capacity *= 2;
        arr->elements = realloc(arr->elements, arr->capacity * sizeof(fl_value_t));
    }

    arr->elements[arr->size++] = args[1];

    return fl_new_int((fl_int)arr->size);
}

fl_value_t fl_array_pop(fl_value_t* args, size_t argc) {
    if (argc != 1 || args[0].type != FL_TYPE_ARRAY) {
        return fl_new_null();
    }

    fl_array_t* arr = args[0].data.array_val;

    if (arr->size == 0) {
        return fl_new_null();
    }

    return arr->elements[--arr->size];
}

fl_value_t fl_array_shift(fl_value_t* args, size_t argc) {
    if (argc != 1 || args[0].type != FL_TYPE_ARRAY) {
        return fl_new_null();
    }

    fl_array_t* arr = args[0].data.array_val;

    if (arr->size == 0) {
        return fl_new_null();
    }

    fl_value_t first = arr->elements[0];

    /* Shift all elements left */
    for (size_t i = 0; i < arr->size - 1; i++) {
        arr->elements[i] = arr->elements[i + 1];
    }
    arr->size--;

    return first;
}

fl_value_t fl_array_unshift(fl_value_t* args, size_t argc) {
    if (argc != 2 || args[0].type != FL_TYPE_ARRAY) {
        return fl_new_null();
    }

    fl_array_t* arr = args[0].data.array_val;

    /* Expand if needed */
    if (arr->size >= arr->capacity) {
        arr->capacity *= 2;
        arr->elements = realloc(arr->elements, arr->capacity * sizeof(fl_value_t));
    }

    /* Shift all elements right */
    for (size_t i = arr->size; i > 0; i--) {
        arr->elements[i] = arr->elements[i - 1];
    }

    arr->elements[0] = args[1];
    arr->size++;

    return fl_new_int((fl_int)arr->size);
}

fl_value_t fl_array_map(fl_value_t* args, size_t argc) {
    /* Array map with callback function
     * Basic implementation: creates new array by applying transformation
     * Full callback support would require function invocation (future)
     * For now, supports simple transformations like string case conversion
     */
    if (argc < 2 || args[0].type != FL_TYPE_ARRAY) {
        return fl_new_null();
    }

    fl_array_t* src_arr = args[0].data.array_val;
    fl_array_t* result_arr = fl_array_create(src_arr->capacity);

    /* If a callback function is provided (future feature), apply it
       For now, return a copy of the array */
    for (size_t i = 0; i < src_arr->size; i++) {
        result_arr->elements[result_arr->size++] = src_arr->elements[i];
    }

    fl_value_t ret;
    ret.type = FL_TYPE_ARRAY;
    ret.data.array_val = result_arr;
    return ret;
}

fl_value_t fl_array_filter(fl_value_t* args, size_t argc) {
    /* Array filter with callback function
     * Basic implementation: filters based on value truthiness or type
     * Full callback support would require function invocation (future)
     */
    if (argc < 2 || args[0].type != FL_TYPE_ARRAY) {
        return fl_new_null();
    }

    fl_array_t* src_arr = args[0].data.array_val;
    fl_array_t* result_arr = fl_array_create(src_arr->capacity);

    /* Filter by truthiness: null, false, 0, "" are falsy */
    for (size_t i = 0; i < src_arr->size; i++) {
        fl_value_t elem = src_arr->elements[i];
        fl_bool is_truthy = true;

        switch (elem.type) {
            case FL_TYPE_NULL:
                is_truthy = false;
                break;
            case FL_TYPE_BOOL:
                is_truthy = elem.data.bool_val;
                break;
            case FL_TYPE_INT:
                is_truthy = elem.data.int_val != 0;
                break;
            case FL_TYPE_FLOAT:
                is_truthy = elem.data.float_val != 0.0;
                break;
            case FL_TYPE_STRING:
                is_truthy = strlen(elem.data.string_val) > 0;
                break;
            default:
                is_truthy = true;  /* Objects, arrays, functions are truthy */
        }

        if (is_truthy) {
            if (result_arr->size >= result_arr->capacity) {
                result_arr->capacity *= 2;
                result_arr->elements = realloc(result_arr->elements, result_arr->capacity * sizeof(fl_value_t));
            }
            result_arr->elements[result_arr->size++] = elem;
        }
    }

    fl_value_t ret;
    ret.type = FL_TYPE_ARRAY;
    ret.data.array_val = result_arr;
    return ret;
}

fl_value_t fl_array_reduce(fl_value_t* args, size_t argc) {
    /* Array reduce with callback function and initial accumulator
     * Basic implementation: numeric sum reduction
     * Full callback support would require function invocation (future)
     */
    if (argc < 2 || args[0].type != FL_TYPE_ARRAY) {
        return fl_new_null();
    }

    fl_array_t* arr = args[0].data.array_val;
    if (arr->size == 0) {
        return fl_new_null();
    }

    /* Initialize accumulator */
    fl_value_t accumulator = fl_new_int(0);
    if (argc > 1) {
        accumulator = args[1];  /* Use provided initial value */
    }

    /* Sum numeric values (basic reduce implementation) */
    for (size_t i = 0; i < arr->size; i++) {
        fl_value_t elem = arr->elements[i];

        if (accumulator.type == FL_TYPE_INT && elem.type == FL_TYPE_INT) {
            accumulator.data.int_val += elem.data.int_val;
        } else if ((accumulator.type == FL_TYPE_INT || accumulator.type == FL_TYPE_FLOAT) &&
                   (elem.type == FL_TYPE_INT || elem.type == FL_TYPE_FLOAT)) {
            double acc_val = (accumulator.type == FL_TYPE_INT) ?
                           (double)accumulator.data.int_val : accumulator.data.float_val;
            double elem_val = (elem.type == FL_TYPE_INT) ?
                           (double)elem.data.int_val : elem.data.float_val;
            accumulator = fl_new_float(acc_val + elem_val);
        } else if (accumulator.type == FL_TYPE_STRING && elem.type == FL_TYPE_STRING) {
            /* String concatenation for reduce */
            size_t new_len = strlen(accumulator.data.string_val) + strlen(elem.data.string_val) + 1;
            char* result = malloc(new_len);
            strcpy(result, accumulator.data.string_val);
            strcat(result, elem.data.string_val);
            if (accumulator.type == FL_TYPE_STRING) {
                free(accumulator.data.string_val);
            }
            accumulator = fl_new_string(result);
            free(result);
        }
    }

    return accumulator;
}

fl_value_t fl_array_join(fl_value_t* args, size_t argc) {
    if (argc != 2 || args[0].type != FL_TYPE_ARRAY || args[1].type != FL_TYPE_STRING) {
        return fl_new_null();
    }

    fl_array_t* arr = args[0].data.array_val;
    const char* separator = args[1].data.string_val;

    if (arr->size == 0) {
        return fl_new_string("");
    }

    /* Calculate total size needed */
    size_t total_size = 0;
    for (size_t i = 0; i < arr->size; i++) {
        if (arr->elements[i].type == FL_TYPE_STRING) {
            total_size += strlen(arr->elements[i].data.string_val);
        } else if (arr->elements[i].type == FL_TYPE_INT || arr->elements[i].type == FL_TYPE_FLOAT) {
            total_size += 20;  /* Estimate for number */
        }
        if (i < arr->size - 1) {
            total_size += strlen(separator);
        }
    }

    char* result = malloc(total_size + 1);
    result[0] = '\0';

    for (size_t i = 0; i < arr->size; i++) {
        if (arr->elements[i].type == FL_TYPE_STRING) {
            strcat(result, arr->elements[i].data.string_val);
        } else if (arr->elements[i].type == FL_TYPE_INT) {
            char buf[20];
            sprintf(buf, "%ld", arr->elements[i].data.int_val);
            strcat(result, buf);
        } else if (arr->elements[i].type == FL_TYPE_FLOAT) {
            char buf[20];
            sprintf(buf, "%g", arr->elements[i].data.float_val);
            strcat(result, buf);
        } else if (arr->elements[i].type == FL_TYPE_BOOL) {
            strcat(result, arr->elements[i].data.bool_val ? "true" : "false");
        }

        if (i < arr->size - 1) {
            strcat(result, separator);
        }
    }

    return fl_new_string(result);
}

fl_value_t fl_array_reverse(fl_value_t* args, size_t argc) {
    if (argc != 1 || args[0].type != FL_TYPE_ARRAY) {
        return fl_new_null();
    }

    fl_array_t* arr = args[0].data.array_val;

    /* Reverse in place */
    for (size_t i = 0; i < arr->size / 2; i++) {
        fl_value_t temp = arr->elements[i];
        arr->elements[i] = arr->elements[arr->size - 1 - i];
        arr->elements[arr->size - 1 - i] = temp;
    }

    fl_value_t ret;
    ret.type = FL_TYPE_ARRAY;
    ret.data.array_val = arr;
    return ret;
}

static int fl_compare_values(const void* a, const void* b) {
    fl_value_t* va = (fl_value_t*)a;
    fl_value_t* vb = (fl_value_t*)b;

    if (va->type == FL_TYPE_INT && vb->type == FL_TYPE_INT) {
        return (va->data.int_val > vb->data.int_val) - (va->data.int_val < vb->data.int_val);
    } else if (va->type == FL_TYPE_FLOAT && vb->type == FL_TYPE_FLOAT) {
        return (va->data.float_val > vb->data.float_val) - (va->data.float_val < vb->data.float_val);
    } else if (va->type == FL_TYPE_STRING && vb->type == FL_TYPE_STRING) {
        return strcmp(va->data.string_val, vb->data.string_val);
    }
    return 0;
}

fl_value_t fl_array_sort(fl_value_t* args, size_t argc) {
    if (argc != 1 || args[0].type != FL_TYPE_ARRAY) {
        return fl_new_null();
    }

    fl_array_t* arr = args[0].data.array_val;

    qsort(arr->elements, arr->size, sizeof(fl_value_t), fl_compare_values);

    fl_value_t ret;
    ret.type = FL_TYPE_ARRAY;
    ret.data.array_val = arr;
    return ret;
}

fl_value_t fl_array_includes(fl_value_t* args, size_t argc) {
    if (argc != 2 || args[0].type != FL_TYPE_ARRAY) {
        return fl_new_null();
    }

    fl_array_t* arr = args[0].data.array_val;
    fl_value_t search_val = args[1];

    for (size_t i = 0; i < arr->size; i++) {
        fl_value_t elem = arr->elements[i];

        if (elem.type != search_val.type) continue;

        if (elem.type == FL_TYPE_INT && elem.data.int_val == search_val.data.int_val) {
            return fl_new_bool(true);
        } else if (elem.type == FL_TYPE_FLOAT && elem.data.float_val == search_val.data.float_val) {
            return fl_new_bool(true);
        } else if (elem.type == FL_TYPE_STRING && strcmp(elem.data.string_val, search_val.data.string_val) == 0) {
            return fl_new_bool(true);
        } else if (elem.type == FL_TYPE_BOOL && elem.data.bool_val == search_val.data.bool_val) {
            return fl_new_bool(true);
        }
    }

    return fl_new_bool(false);
}

fl_value_t fl_array_slice(fl_value_t* args, size_t argc) {
    if (argc != 3 || args[0].type != FL_TYPE_ARRAY) {
        return fl_new_null();
    }

    fl_array_t* arr = args[0].data.array_val;
    fl_int start = (args[1].type == FL_TYPE_INT) ? args[1].data.int_val : 0;
    fl_int end = (args[2].type == FL_TYPE_INT) ? args[2].data.int_val : (fl_int)arr->size;

    if (start < 0) start = 0;
    if (end > (fl_int)arr->size) end = (fl_int)arr->size;
    if (start > end) start = end;

    fl_array_t* new_arr = fl_array_create(10);

    for (fl_int i = start; i < end; i++) {
        if (new_arr->size >= new_arr->capacity) {
            new_arr->capacity *= 2;
            new_arr->elements = realloc(new_arr->elements, new_arr->capacity * sizeof(fl_value_t));
        }
        new_arr->elements[new_arr->size++] = arr->elements[i];
    }

    fl_value_t ret;
    ret.type = FL_TYPE_ARRAY;
    ret.data.array_val = new_arr;
    return ret;
}

fl_value_t fl_array_index_of(fl_value_t* args, size_t argc) {
    if (argc != 2 || args[0].type != FL_TYPE_ARRAY) {
        return fl_new_null();
    }

    fl_array_t* arr = args[0].data.array_val;
    fl_value_t search_val = args[1];

    for (size_t i = 0; i < arr->size; i++) {
        fl_value_t elem = arr->elements[i];

        if (elem.type == FL_TYPE_INT && search_val.type == FL_TYPE_INT &&
            elem.data.int_val == search_val.data.int_val) {
            return fl_new_int((fl_int)i);
        } else if (elem.type == FL_TYPE_STRING && search_val.type == FL_TYPE_STRING &&
                   strcmp(elem.data.string_val, search_val.data.string_val) == 0) {
            return fl_new_int((fl_int)i);
        }
    }

    return fl_new_int(-1);
}

fl_value_t fl_array_last_index_of(fl_value_t* args, size_t argc) {
    if (argc != 2 || args[0].type != FL_TYPE_ARRAY) {
        return fl_new_null();
    }

    fl_array_t* arr = args[0].data.array_val;
    fl_value_t search_val = args[1];

    for (fl_int i = (fl_int)arr->size - 1; i >= 0; i--) {
        fl_value_t elem = arr->elements[i];

        if (elem.type == FL_TYPE_INT && search_val.type == FL_TYPE_INT &&
            elem.data.int_val == search_val.data.int_val) {
            return fl_new_int(i);
        } else if (elem.type == FL_TYPE_STRING && search_val.type == FL_TYPE_STRING &&
                   strcmp(elem.data.string_val, search_val.data.string_val) == 0) {
            return fl_new_int(i);
        }
    }

    return fl_new_int(-1);
}

/* Math Functions */

fl_value_t fl_math_abs(fl_value_t* args, size_t argc) {
    if (argc != 1) {
        return fl_new_null();
    }

    if (args[0].type == FL_TYPE_INT) {
        fl_int val = args[0].data.int_val;
        return fl_new_int(val < 0 ? -val : val);
    } else if (args[0].type == FL_TYPE_FLOAT) {
        return fl_new_float(fabs(args[0].data.float_val));
    }

    return fl_new_null();
}

fl_value_t fl_math_sqrt(fl_value_t* args, size_t argc) {
    if (argc != 1) {
        return fl_new_null();
    }

    double val = 0.0;
    if (args[0].type == FL_TYPE_INT) {
        val = (double)args[0].data.int_val;
    } else if (args[0].type == FL_TYPE_FLOAT) {
        val = args[0].data.float_val;
    } else {
        return fl_new_null();
    }

    return fl_new_float(sqrt(val));
}

fl_value_t fl_math_pow(fl_value_t* args, size_t argc) {
    if (argc != 2) {
        return fl_new_null();
    }

    double base = 0.0, exp = 0.0;

    if (args[0].type == FL_TYPE_INT) {
        base = (double)args[0].data.int_val;
    } else if (args[0].type == FL_TYPE_FLOAT) {
        base = args[0].data.float_val;
    } else {
        return fl_new_null();
    }

    if (args[1].type == FL_TYPE_INT) {
        exp = (double)args[1].data.int_val;
    } else if (args[1].type == FL_TYPE_FLOAT) {
        exp = args[1].data.float_val;
    } else {
        return fl_new_null();
    }

    return fl_new_float(pow(base, exp));
}

fl_value_t fl_math_floor(fl_value_t* args, size_t argc) {
    if (argc != 1) {
        return fl_new_null();
    }

    double val = 0.0;
    if (args[0].type == FL_TYPE_INT) {
        val = (double)args[0].data.int_val;
    } else if (args[0].type == FL_TYPE_FLOAT) {
        val = args[0].data.float_val;
    } else {
        return fl_new_null();
    }

    return fl_new_float(floor(val));
}

fl_value_t fl_math_ceil(fl_value_t* args, size_t argc) {
    if (argc != 1) {
        return fl_new_null();
    }

    double val = 0.0;
    if (args[0].type == FL_TYPE_INT) {
        val = (double)args[0].data.int_val;
    } else if (args[0].type == FL_TYPE_FLOAT) {
        val = args[0].data.float_val;
    } else {
        return fl_new_null();
    }

    return fl_new_float(ceil(val));
}

fl_value_t fl_math_round(fl_value_t* args, size_t argc) {
    if (argc != 1) {
        return fl_new_null();
    }

    double val = 0.0;
    if (args[0].type == FL_TYPE_INT) {
        return args[0];  /* Already an integer */
    } else if (args[0].type == FL_TYPE_FLOAT) {
        val = args[0].data.float_val;
    } else {
        return fl_new_null();
    }

    return fl_new_float(round(val));
}

fl_value_t fl_math_min(fl_value_t* args, size_t argc) {
    if (argc < 2) {
        return fl_new_null();
    }

    double min_val = 1e308;

    for (size_t i = 0; i < argc; i++) {
        if (args[i].type == FL_TYPE_INT) {
            double val = (double)args[i].data.int_val;
            if (val < min_val) min_val = val;
        } else if (args[i].type == FL_TYPE_FLOAT) {
            if (args[i].data.float_val < min_val) {
                min_val = args[i].data.float_val;
            }
        }
    }

    if (min_val == 1e308) return fl_new_null();

    return fl_new_float(min_val);
}

fl_value_t fl_math_max(fl_value_t* args, size_t argc) {
    if (argc < 2) {
        return fl_new_null();
    }

    double max_val = -1e308;

    for (size_t i = 0; i < argc; i++) {
        if (args[i].type == FL_TYPE_INT) {
            double val = (double)args[i].data.int_val;
            if (val > max_val) max_val = val;
        } else if (args[i].type == FL_TYPE_FLOAT) {
            if (args[i].data.float_val > max_val) {
                max_val = args[i].data.float_val;
            }
        }
    }

    if (max_val == -1e308) return fl_new_null();

    return fl_new_float(max_val);
}

fl_value_t fl_math_random(fl_value_t* args, size_t argc) {
    /* Return random float between 0.0 and 1.0 */
    return fl_new_float((double)rand() / RAND_MAX);
}

fl_value_t fl_math_sin(fl_value_t* args, size_t argc) {
    if (argc != 1) {
        return fl_new_null();
    }

    double val = 0.0;
    if (args[0].type == FL_TYPE_INT) {
        val = (double)args[0].data.int_val;
    } else if (args[0].type == FL_TYPE_FLOAT) {
        val = args[0].data.float_val;
    } else {
        return fl_new_null();
    }

    return fl_new_float(sin(val));
}

fl_value_t fl_math_cos(fl_value_t* args, size_t argc) {
    if (argc != 1) {
        return fl_new_null();
    }

    double val = 0.0;
    if (args[0].type == FL_TYPE_INT) {
        val = (double)args[0].data.int_val;
    } else if (args[0].type == FL_TYPE_FLOAT) {
        val = args[0].data.float_val;
    } else {
        return fl_new_null();
    }

    return fl_new_float(cos(val));
}

fl_value_t fl_math_tan(fl_value_t* args, size_t argc) {
    if (argc != 1) {
        return fl_new_null();
    }

    double val = 0.0;
    if (args[0].type == FL_TYPE_INT) {
        val = (double)args[0].data.int_val;
    } else if (args[0].type == FL_TYPE_FLOAT) {
        val = args[0].data.float_val;
    } else {
        return fl_new_null();
    }

    return fl_new_float(tan(val));
}

fl_value_t fl_math_log(fl_value_t* args, size_t argc) {
    if (argc != 1) {
        return fl_new_null();
    }

    double val = 0.0;
    if (args[0].type == FL_TYPE_INT) {
        val = (double)args[0].data.int_val;
    } else if (args[0].type == FL_TYPE_FLOAT) {
        val = args[0].data.float_val;
    } else {
        return fl_new_null();
    }

    return fl_new_float(log(val));
}

fl_value_t fl_math_exp(fl_value_t* args, size_t argc) {
    if (argc != 1) {
        return fl_new_null();
    }

    double val = 0.0;
    if (args[0].type == FL_TYPE_INT) {
        val = (double)args[0].data.int_val;
    } else if (args[0].type == FL_TYPE_FLOAT) {
        val = args[0].data.float_val;
    } else {
        return fl_new_null();
    }

    return fl_new_float(exp(val));
}

/* Type Functions */

fl_value_t fl_typeof(fl_value_t* args, size_t argc) {
    if (argc != 1) {
        return fl_new_null();
    }

    const char* type_str = fl_type_name(args[0].type);
    fl_value_t result;
    result.type = FL_TYPE_STRING;
    result.data.string_val = malloc(strlen(type_str) + 1);
    strcpy(result.data.string_val, type_str);
    return result;
}

fl_value_t fl_string_convert(fl_value_t* args, size_t argc) {
    if (argc != 1) {
        return fl_new_null();
    }

    char buffer[256];

    switch (args[0].type) {
        case FL_TYPE_NULL:
            return fl_new_string("null");
        case FL_TYPE_BOOL:
            return fl_new_string(args[0].data.bool_val ? "true" : "false");
        case FL_TYPE_INT:
            sprintf(buffer, "%ld", args[0].data.int_val);
            return fl_new_string(buffer);
        case FL_TYPE_FLOAT:
            sprintf(buffer, "%g", args[0].data.float_val);
            return fl_new_string(buffer);
        case FL_TYPE_STRING:
            return fl_new_string(args[0].data.string_val);
        case FL_TYPE_ARRAY:
            return fl_new_string("[array]");
        case FL_TYPE_OBJECT:
            return fl_new_string("{object}");
        case FL_TYPE_FUNCTION:
            return fl_new_string("[function]");
        default:
            return fl_new_null();
    }
}

fl_value_t fl_number_convert(fl_value_t* args, size_t argc) {
    if (argc != 1) {
        return fl_new_null();
    }

    switch (args[0].type) {
        case FL_TYPE_INT:
            return fl_new_float((double)args[0].data.int_val);
        case FL_TYPE_FLOAT:
            return args[0];
        case FL_TYPE_BOOL:
            return fl_new_float(args[0].data.bool_val ? 1.0 : 0.0);
        case FL_TYPE_STRING: {
            char* endptr;
            double val = strtod(args[0].data.string_val, &endptr);
            return fl_new_float(val);
        }
        default:
            return fl_new_null();
    }
}

fl_value_t fl_bool_convert(fl_value_t* args, size_t argc) {
    if (argc != 1) {
        return fl_new_null();
    }

    switch (args[0].type) {
        case FL_TYPE_NULL:
            return fl_new_bool(false);
        case FL_TYPE_BOOL:
            return args[0];
        case FL_TYPE_INT:
            return fl_new_bool(args[0].data.int_val != 0);
        case FL_TYPE_FLOAT:
            return fl_new_bool(args[0].data.float_val != 0.0);
        case FL_TYPE_STRING:
            return fl_new_bool(strlen(args[0].data.string_val) > 0);
        case FL_TYPE_ARRAY:
        case FL_TYPE_OBJECT:
        case FL_TYPE_FUNCTION:
            return fl_new_bool(true);  /* Non-null objects are truthy */
        default:
            return fl_new_null();
    }
}

fl_value_t fl_is_null(fl_value_t* args, size_t argc) {
    if (argc != 1) {
        return fl_new_null();
    }

    return fl_new_bool(args[0].type == FL_TYPE_NULL);
}

fl_value_t fl_array_from(fl_value_t* args, size_t argc) {
    if (argc != 1) {
        return fl_new_null();
    }

    /* Convert various types to array */
    switch (args[0].type) {
        case FL_TYPE_ARRAY:
            return args[0];  /* Already array */
        case FL_TYPE_STRING: {
            const char* str = args[0].data.string_val;
            fl_array_t* arr = fl_array_create(strlen(str));
            for (size_t i = 0; str[i]; i++) {
                char ch[2] = {str[i], '\0'};
                fl_value_t elem = fl_new_string(ch);
                arr->elements[arr->size++] = elem;
            }
            fl_value_t ret;
            ret.type = FL_TYPE_ARRAY;
            ret.data.array_val = arr;
            return ret;
        }
        default:
            /* Single element array */
            fl_array_t* arr = fl_array_create(1);
            arr->elements[0] = args[0];
            arr->size = 1;
            fl_value_t ret;
            ret.type = FL_TYPE_ARRAY;
            ret.data.array_val = arr;
            return ret;
    }
}

/* Control Flow */

fl_value_t fl_assert(fl_value_t* args, size_t argc) {
    if (argc < 1) {
        return fl_new_null();
    }

    fl_bool condition = false;

    /* Evaluate truthiness */
    if (args[0].type == FL_TYPE_BOOL) {
        condition = args[0].data.bool_val;
    } else if (args[0].type == FL_TYPE_INT) {
        condition = args[0].data.int_val != 0;
    } else if (args[0].type == FL_TYPE_FLOAT) {
        condition = args[0].data.float_val != 0.0;
    } else if (args[0].type == FL_TYPE_STRING) {
        condition = strlen(args[0].data.string_val) > 0;
    } else if (args[0].type == FL_TYPE_NULL) {
        condition = false;
    } else {
        condition = true;  /* Objects, arrays, functions are truthy */
    }

    if (!condition) {
        const char* msg = "assertion failed";
        if (argc > 1 && args[1].type == FL_TYPE_STRING) {
            msg = args[1].data.string_val;
        }
        fprintf(stderr, "AssertionError: %s\n", msg);
        exit(1);
    }

    return fl_new_null();
}

fl_value_t fl_panic(fl_value_t* args, size_t argc) {
    const char* msg = "panic";
    if (argc > 0 && args[0].type == FL_TYPE_STRING) {
        msg = args[0].data.string_val;
    }

    fprintf(stderr, "panic: %s\n", msg);
    exit(1);

    return fl_new_null();  /* Never reached */
}

fl_value_t fl_object_keys(fl_value_t* args, size_t argc) {
    if (argc != 1 || args[0].type != FL_TYPE_OBJECT) {
        return fl_new_null();
    }

    fl_object_t* obj = args[0].data.object_val;
    fl_array_t* arr = fl_array_create(obj->size);

    for (size_t i = 0; i < obj->size; i++) {
        fl_value_t key = fl_new_string(obj->keys[i]);
        arr->elements[arr->size++] = key;
    }

    fl_value_t ret;
    ret.type = FL_TYPE_ARRAY;
    ret.data.array_val = arr;
    return ret;
}

fl_value_t fl_object_values(fl_value_t* args, size_t argc) {
    if (argc != 1 || args[0].type != FL_TYPE_OBJECT) {
        return fl_new_null();
    }

    fl_object_t* obj = args[0].data.object_val;
    fl_array_t* arr = fl_array_create(obj->size);

    for (size_t i = 0; i < obj->size; i++) {
        arr->elements[arr->size++] = obj->values[i];
    }

    fl_value_t ret;
    ret.type = FL_TYPE_ARRAY;
    ret.data.array_val = arr;
    return ret;
}

fl_value_t fl_object_entries(fl_value_t* args, size_t argc) {
    if (argc != 1 || args[0].type != FL_TYPE_OBJECT) {
        return fl_new_null();
    }

    fl_object_t* obj = args[0].data.object_val;
    fl_array_t* arr = fl_array_create(obj->size);

    for (size_t i = 0; i < obj->size; i++) {
        fl_array_t* entry = fl_array_create(2);
        entry->elements[0] = fl_new_string(obj->keys[i]);
        entry->elements[1] = obj->values[i];
        entry->size = 2;

        fl_value_t entry_val;
        entry_val.type = FL_TYPE_ARRAY;
        entry_val.data.array_val = entry;

        arr->elements[arr->size++] = entry_val;
    }

    fl_value_t ret;
    ret.type = FL_TYPE_ARRAY;
    ret.data.array_val = arr;
    return ret;
}

fl_value_t fl_object_assign(fl_value_t* args, size_t argc) {
    if (argc < 2 || args[0].type != FL_TYPE_OBJECT) {
        return fl_new_null();
    }

    fl_object_t* target = args[0].data.object_val;

    for (size_t i = 1; i < argc; i++) {
        if (args[i].type != FL_TYPE_OBJECT) continue;

        fl_object_t* source = args[i].data.object_val;
        for (size_t j = 0; j < source->size; j++) {
            /* Find or add key */
            int found = -1;
            for (size_t k = 0; k < target->size; k++) {
                if (strcmp(target->keys[k], source->keys[j]) == 0) {
                    found = (int)k;
                    break;
                }
            }

            if (found >= 0) {
                target->values[found] = source->values[j];
            } else {
                /* Add new key */
                if (target->size >= target->capacity) {
                    target->capacity *= 2;
                    target->keys = realloc(target->keys, target->capacity * sizeof(char*));
                    target->values = realloc(target->values, target->capacity * sizeof(fl_value_t));
                }
                target->keys[target->size] = malloc(strlen(source->keys[j]) + 1);
                strcpy(target->keys[target->size], source->keys[j]);
                target->values[target->size] = source->values[j];
                target->size++;
            }
        }
    }

    return args[0];
}

fl_value_t fl_json_stringify(fl_value_t* args, size_t argc) {
    if (argc != 1) {
        return fl_new_null();
    }

    /* Simple JSON stringification */
    char buffer[4096] = {0};

    switch (args[0].type) {
        case FL_TYPE_NULL:
            strcpy(buffer, "null");
            break;
        case FL_TYPE_BOOL:
            strcpy(buffer, args[0].data.bool_val ? "true" : "false");
            break;
        case FL_TYPE_INT:
            sprintf(buffer, "%ld", args[0].data.int_val);
            break;
        case FL_TYPE_FLOAT:
            sprintf(buffer, "%g", args[0].data.float_val);
            break;
        case FL_TYPE_STRING:
            sprintf(buffer, "\"%s\"", args[0].data.string_val);
            break;
        case FL_TYPE_ARRAY: {
            fl_array_t* arr = args[0].data.array_val;
            strcat(buffer, "[");
            for (size_t i = 0; i < arr->size; i++) {
                if (i > 0) strcat(buffer, ",");
                /* Recursively stringify elements */
                fl_value_t elem_result = fl_json_stringify(&arr->elements[i], 1);
                if (elem_result.type == FL_TYPE_STRING) {
                    strcat(buffer, elem_result.data.string_val);
                    free(elem_result.data.string_val);
                }
            }
            strcat(buffer, "]");
            break;
        }
        case FL_TYPE_OBJECT: {
            fl_object_t* obj = args[0].data.object_val;
            strcat(buffer, "{");
            for (size_t i = 0; i < obj->size; i++) {
                if (i > 0) strcat(buffer, ",");
                sprintf(buffer + strlen(buffer), "\"%s\":", obj->keys[i]);
                fl_value_t val_result = fl_json_stringify(&obj->values[i], 1);
                if (val_result.type == FL_TYPE_STRING) {
                    strcat(buffer, val_result.data.string_val);
                    free(val_result.data.string_val);
                }
            }
            strcat(buffer, "}");
            break;
        }
        default:
            strcpy(buffer, "null");
    }

    return fl_new_string(buffer);
}

fl_value_t fl_json_parse(fl_value_t* args, size_t argc) {
    /* JSON parsing - basic implementation
     * Supports: null, booleans, numbers, strings
     * Partial support for arrays and objects
     */
    if (argc != 1 || args[0].type != FL_TYPE_STRING) {
        return fl_new_null();
    }

    const char* json_str = args[0].data.string_val;

    /* Skip whitespace */
    while (*json_str && isspace(*json_str)) json_str++;

    if (!*json_str) {
        return fl_new_null();
    }

    /* Parse null */
    if (strncmp(json_str, "null", 4) == 0) {
        return fl_new_null();
    }

    /* Parse booleans */
    if (strncmp(json_str, "true", 4) == 0) {
        return fl_new_bool(true);
    }
    if (strncmp(json_str, "false", 5) == 0) {
        return fl_new_bool(false);
    }

    /* Parse numbers */
    if (isdigit(*json_str) || *json_str == '-' || *json_str == '+') {
        char* endptr;

        /* Try parsing as float first */
        double d = strtod(json_str, &endptr);
        if (endptr != json_str) {
            /* Check if it's actually an integer */
            if (d == (double)(int64_t)d && strchr(json_str, '.') == NULL) {
                return fl_new_int((fl_int)d);
            }
            return fl_new_float(d);
        }
    }

    /* Parse strings (JSON strings) */
    if (*json_str == '"') {
        json_str++;
        char buffer[4096];
        size_t idx = 0;

        while (*json_str && *json_str != '"' && idx < sizeof(buffer) - 1) {
            if (*json_str == '\\') {
                json_str++;
                switch (*json_str) {
                    case 'n': buffer[idx++] = '\n'; break;
                    case 't': buffer[idx++] = '\t'; break;
                    case 'r': buffer[idx++] = '\r'; break;
                    case '"': buffer[idx++] = '"'; break;
                    case '\\': buffer[idx++] = '\\'; break;
                    case '/': buffer[idx++] = '/'; break;
                    case 'b': buffer[idx++] = '\b'; break;
                    case 'f': buffer[idx++] = '\f'; break;
                    default: buffer[idx++] = *json_str;
                }
                json_str++;
            } else {
                buffer[idx++] = *json_str++;
            }
        }
        buffer[idx] = '\0';
        return fl_new_string(buffer);
    }

    /* Parse arrays - basic implementation */
    if (*json_str == '[') {
        fl_array_t* arr = fl_array_create(10);
        json_str++;

        while (*json_str && *json_str != ']') {
            while (*json_str && isspace(*json_str)) json_str++;
            if (*json_str == ']') break;

            /* Recursively parse array elements */
            /* For simplicity, skip nested structures for now */
            if (*json_str == ',') {
                json_str++;
            } else if (*json_str == '"') {
                /* Parse string element */
                json_str++;
                char buffer[1024];
                size_t idx = 0;
                while (*json_str && *json_str != '"' && idx < sizeof(buffer) - 1) {
                    buffer[idx++] = *json_str++;
                }
                buffer[idx] = '\0';
                if (*json_str == '"') json_str++;

                fl_value_t elem = fl_new_string(buffer);
                if (arr->size >= arr->capacity) {
                    arr->capacity *= 2;
                    arr->elements = realloc(arr->elements, arr->capacity * sizeof(fl_value_t));
                }
                arr->elements[arr->size++] = elem;
            } else {
                json_str++;
            }
        }

        fl_value_t ret;
        ret.type = FL_TYPE_ARRAY;
        ret.data.array_val = arr;
        return ret;
    }

    /* Default: return the original string if parsing failed */
    return args[0];
}

/* Register all stdlib functions */

void fl_stdlib_register(fl_runtime_t* runtime) {
    /* Register all built-in functions with the runtime
     * This allows FreeLang programs to call these functions by name
     *
     * Usage: Functions are called from interpreter/compiler as:
     *   println("hello")   -> calls fl_println
     *   x = len(arr)       -> calls fl_array_length
     *   s = upper("abc")   -> calls fl_string_upper
     */

    if (!runtime) {
        return;
    }

    /* I/O Functions (3) */
    /* print/println implemented in VM's builtin handler */
    /* input/output operations use stdio directly */

    /* String Functions (16) */
    /* Registered in runtime as:
     *   len(str) -> fl_string_length
     *   upper(str) -> fl_string_upper
     *   lower(str) -> fl_string_lower
     *   contains(str, substr) -> fl_string_contains
     *   etc.
     */

    /* Array Functions (15) */
    /* Registered in runtime as:
     *   len(arr) -> fl_array_length
     *   push(arr, val) -> fl_array_push
     *   pop(arr) -> fl_array_pop
     *   map(arr, fn) -> fl_array_map
     *   filter(arr, fn) -> fl_array_filter
     *   reduce(arr, fn, init) -> fl_array_reduce
     *   join(arr, sep) -> fl_array_join
     *   etc.
     */

    /* Math Functions (15) */
    /* Registered as:
     *   abs(x) -> fl_math_abs
     *   sqrt(x) -> fl_math_sqrt
     *   pow(x, y) -> fl_math_pow
     *   sin(x) -> fl_math_sin
     *   cos(x) -> fl_math_cos
     *   random() -> fl_math_random
     *   etc.
     */

    /* Type Functions (6) */
    /* typeof(val) -> fl_typeof
     * int(val) -> fl_number_convert
     * str(val) -> fl_string_convert
     * bool(val) -> fl_bool_convert
     * etc.
     */

    /* Object Functions (4) */
    /* keys(obj) -> fl_object_keys
     * values(obj) -> fl_object_values
     * entries(obj) -> fl_object_entries
     * assign(obj, ...) -> fl_object_assign
     */

    /* JSON Functions (2) */
    /* json.stringify(val) -> fl_json_stringify
     * json.parse(str) -> fl_json_parse
     */

    /* Control Flow (2) */
    /* assert(cond, msg) -> fl_assert
     * panic(msg) -> fl_panic
     */

    /* Note: Actual registration depends on the runtime's function dispatch mechanism.
     * The functions are implemented above and can be called directly or through
     * a function lookup table in the runtime/VM.
     */

    /* Crypto Functions (Phase 5) */
    /* sha256(bytes|string) -> bytes[32]
     * hmac_sha256(key, data) -> bytes[32]
     * pbkdf2(password, salt, iterations, dklen) -> bytes
     * crypto_random(n) -> bytes[n]
     * bytes_to_hex(bytes) -> string
     * crypto_compare(a, b) -> bool
     * u32_rotr(val, n) -> u32
     * u32_add(a, b) -> u32
     */

    /* Proof-Logger Functions (Phase 9) */
    fl_runtime_register_builtin(runtime, "log_configure", fl_log_configure_builtin);
    fl_runtime_register_builtin(runtime, "log_debug",     fl_log_debug_builtin);
    fl_runtime_register_builtin(runtime, "log_info",      fl_log_info_builtin);
    fl_runtime_register_builtin(runtime, "log_warn",      fl_log_warn_builtin);
    fl_runtime_register_builtin(runtime, "log_error",     fl_log_error_builtin);
    fl_runtime_register_builtin(runtime, "log_flush",     fl_log_flush_builtin);
    fl_runtime_register_builtin(runtime, "log_stats",     fl_log_stats_builtin);

    /* Compression Functions (MOSS-Compressor v1.0) */
    fl_runtime_register_builtin(runtime, "compress",       fl_compress);
    fl_runtime_register_builtin(runtime, "decompress",     fl_decompress);
    fl_runtime_register_builtin(runtime, "gzip",           fl_gzip);
    fl_runtime_register_builtin(runtime, "gunzip",         fl_gunzip);
    fl_runtime_register_builtin(runtime, "compress_ratio", fl_compress_ratio);
    fl_runtime_register_builtin(runtime, "compress_info",  fl_compress_info);

    /* MOSS-Autodoc Functions (Phase 8) - Self-Documenting API (Swagger 대체) */
    fl_runtime_register_builtin(runtime, "autodoc_init",        fl_autodoc_init_builtin);
    fl_runtime_register_builtin(runtime, "autodoc_register",    fl_autodoc_register_builtin);
    fl_runtime_register_builtin(runtime, "autodoc_add_param",   fl_autodoc_add_param_builtin);
    fl_runtime_register_builtin(runtime, "autodoc_json",        fl_autodoc_json_builtin);
    fl_runtime_register_builtin(runtime, "autodoc_html",        fl_autodoc_html_builtin);
    fl_runtime_register_builtin(runtime, "autodoc_routes_json", fl_autodoc_routes_json_builtin);
    fl_runtime_register_builtin(runtime, "autodoc_count",       fl_autodoc_count_builtin);
}

/* ============================================================================
   PHASE 5: Crypto Standard Library — SHA-256, HMAC, PBKDF2
   FIPS 180-4, RFC 2104, RFC 8018
   Zero external dependencies. Pure C implementation.
   ============================================================================ */

/* --- SHA-256 Constants (FIPS 180-4 §4.2.2) --- */
static const uint32_t SHA256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/* SHA-256 initial hash values (FIPS 180-4 §5.3.3) */
static const uint32_t SHA256_H0[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

#define SHA256_ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define SHA256_CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define SHA256_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA256_SIG0(x) (SHA256_ROTR(x,  2) ^ SHA256_ROTR(x, 13) ^ SHA256_ROTR(x, 22))
#define SHA256_SIG1(x) (SHA256_ROTR(x,  6) ^ SHA256_ROTR(x, 11) ^ SHA256_ROTR(x, 25))
#define SHA256_GAM0(x) (SHA256_ROTR(x,  7) ^ SHA256_ROTR(x, 18) ^ ((x) >>  3))
#define SHA256_GAM1(x) (SHA256_ROTR(x, 17) ^ SHA256_ROTR(x, 19) ^ ((x) >> 10))

/* Internal: compress one 512-bit block into hash state */
static void sha256_compress(uint32_t state[8], const uint8_t block[64]) {
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h, t1, t2;
    int i;

    /* Message schedule */
    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i*4]     << 24)
             | ((uint32_t)block[i*4 + 1] << 16)
             | ((uint32_t)block[i*4 + 2] <<  8)
             | ((uint32_t)block[i*4 + 3]);
    }
    for (i = 16; i < 64; i++) {
        w[i] = SHA256_GAM1(w[i-2]) + w[i-7] + SHA256_GAM0(w[i-15]) + w[i-16];
    }

    /* Initialize working variables */
    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];

    /* 64 rounds */
    for (i = 0; i < 64; i++) {
        t1 = h + SHA256_SIG1(e) + SHA256_CH(e, f, g) + SHA256_K[i] + w[i];
        t2 = SHA256_SIG0(a) + SHA256_MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

/* Core SHA-256: hash raw bytes, write 32-byte digest into out */
static void sha256_raw(const uint8_t *data, size_t len, uint8_t out[32]) {
    uint32_t state[8];
    uint8_t block[64];
    size_t i;

    memcpy(state, SHA256_H0, sizeof(state));

    /* Process full blocks */
    size_t offset = 0;
    while (offset + 64 <= len) {
        sha256_compress(state, data + offset);
        offset += 64;
    }

    /* Final block(s) with padding (FIPS 180-4 §5.1.1) */
    size_t remain = len - offset;
    memcpy(block, data + offset, remain);
    block[remain] = 0x80;
    memset(block + remain + 1, 0, 64 - remain - 1);

    if (remain >= 56) {
        /* Need extra block */
        sha256_compress(state, block);
        memset(block, 0, 56);
    }

    /* Append bit length (big-endian 64-bit) */
    uint64_t bit_len = (uint64_t)len * 8;
    for (i = 0; i < 8; i++) {
        block[63 - i] = (uint8_t)(bit_len >> (i * 8));
    }
    sha256_compress(state, block);

    /* Output digest (big-endian) */
    for (i = 0; i < 8; i++) {
        out[i*4]     = (uint8_t)(state[i] >> 24);
        out[i*4 + 1] = (uint8_t)(state[i] >> 16);
        out[i*4 + 2] = (uint8_t)(state[i] >>  8);
        out[i*4 + 3] = (uint8_t)(state[i]);
    }
}

/* Helper: make fl_value_t from raw 32-byte digest */
static fl_value_t make_bytes_from_raw(const uint8_t *data, size_t len) {
    fl_bytes_t *b = malloc(sizeof(fl_bytes_t));
    b->data = malloc(len);
    memcpy(b->data, data, len);
    b->size = len;
    b->capacity = len;
    fl_value_t result;
    result.type = FL_TYPE_BYTES;
    result.data.bytes_val = b;
    return result;
}

/* --- FreeLang binding: sha256(bytes|string) -> bytes[32] --- */
fl_value_t fl_sha256(fl_value_t *args, size_t argc) {
    if (argc < 1) {
        fprintf(stderr, "[CRYPTO] sha256: requires 1 argument\n");
        fl_value_t v; v.type = FL_TYPE_NULL; return v;
    }

    const uint8_t *data = NULL;
    size_t len = 0;
    uint8_t str_buf[0]; /* placeholder */

    if (args[0].type == FL_TYPE_BYTES && args[0].data.bytes_val) {
        data = args[0].data.bytes_val->data;
        len  = args[0].data.bytes_val->size;
    } else if (args[0].type == FL_TYPE_STRING && args[0].data.string_val) {
        data = (const uint8_t *)args[0].data.string_val;
        len  = strlen(args[0].data.string_val);
    } else {
        fprintf(stderr, "[CRYPTO] sha256: requires bytes or string\n");
        fl_value_t v; v.type = FL_TYPE_NULL; return v;
    }

    uint8_t digest[32];
    sha256_raw(data, len, digest);
    return make_bytes_from_raw(digest, 32);
}

/* --- HMAC-SHA256 (RFC 2104) --- */
static void hmac_sha256_raw(const uint8_t *key, size_t key_len,
                             const uint8_t *msg, size_t msg_len,
                             uint8_t out[32]) {
    uint8_t k_ipad[64], k_opad[64];
    uint8_t k_block[32];

    /* If key > 64 bytes, hash it first */
    if (key_len > 64) {
        sha256_raw(key, key_len, k_block);
        key = k_block;
        key_len = 32;
    }

    memset(k_ipad, 0x36, 64);
    memset(k_opad, 0x5c, 64);
    for (size_t i = 0; i < key_len; i++) {
        k_ipad[i] ^= key[i];
        k_opad[i] ^= key[i];
    }

    /* inner = SHA256(k_ipad || msg) */
    size_t inner_len = 64 + msg_len;
    uint8_t *inner_buf = malloc(inner_len);
    memcpy(inner_buf, k_ipad, 64);
    memcpy(inner_buf + 64, msg, msg_len);
    uint8_t inner_hash[32];
    sha256_raw(inner_buf, inner_len, inner_hash);
    free(inner_buf);

    /* out = SHA256(k_opad || inner_hash) */
    uint8_t outer_buf[64 + 32];
    memcpy(outer_buf, k_opad, 64);
    memcpy(outer_buf + 64, inner_hash, 32);
    sha256_raw(outer_buf, 64 + 32, out);
}

fl_value_t fl_hmac_sha256(fl_value_t *args, size_t argc) {
    if (argc < 2) {
        fprintf(stderr, "[CRYPTO] hmac_sha256: requires 2 arguments (key, data)\n");
        fl_value_t v; v.type = FL_TYPE_NULL; return v;
    }

    const uint8_t *key_data = NULL; size_t key_len = 0;
    const uint8_t *msg_data = NULL; size_t msg_len = 0;

    if (args[0].type == FL_TYPE_BYTES && args[0].data.bytes_val) {
        key_data = args[0].data.bytes_val->data;
        key_len  = args[0].data.bytes_val->size;
    } else if (args[0].type == FL_TYPE_STRING && args[0].data.string_val) {
        key_data = (const uint8_t *)args[0].data.string_val;
        key_len  = strlen(args[0].data.string_val);
    }

    if (args[1].type == FL_TYPE_BYTES && args[1].data.bytes_val) {
        msg_data = args[1].data.bytes_val->data;
        msg_len  = args[1].data.bytes_val->size;
    } else if (args[1].type == FL_TYPE_STRING && args[1].data.string_val) {
        msg_data = (const uint8_t *)args[1].data.string_val;
        msg_len  = strlen(args[1].data.string_val);
    }

    if (!key_data || !msg_data) {
        fprintf(stderr, "[CRYPTO] hmac_sha256: invalid argument types\n");
        fl_value_t v; v.type = FL_TYPE_NULL; return v;
    }

    uint8_t mac[32];
    hmac_sha256_raw(key_data, key_len, msg_data, msg_len, mac);
    return make_bytes_from_raw(mac, 32);
}

/* --- PBKDF2-HMAC-SHA256 (RFC 8018 §5.2) — bcrypt 대체 --- */
fl_value_t fl_pbkdf2_hmac_sha256(fl_value_t *args, size_t argc) {
    if (argc < 4) {
        fprintf(stderr, "[CRYPTO] pbkdf2: requires 4 arguments (password, salt, iterations, dklen)\n");
        fl_value_t v; v.type = FL_TYPE_NULL; return v;
    }

    const uint8_t *pwd  = NULL; size_t pwd_len  = 0;
    const uint8_t *salt = NULL; size_t salt_len = 0;
    size_t iterations = 10000;
    size_t dklen = 32;

    if (args[0].type == FL_TYPE_BYTES && args[0].data.bytes_val) {
        pwd = args[0].data.bytes_val->data; pwd_len = args[0].data.bytes_val->size;
    } else if (args[0].type == FL_TYPE_STRING && args[0].data.string_val) {
        pwd = (const uint8_t *)args[0].data.string_val;
        pwd_len = strlen(args[0].data.string_val);
    }
    if (args[1].type == FL_TYPE_BYTES && args[1].data.bytes_val) {
        salt = args[1].data.bytes_val->data; salt_len = args[1].data.bytes_val->size;
    } else if (args[1].type == FL_TYPE_STRING && args[1].data.string_val) {
        salt = (const uint8_t *)args[1].data.string_val;
        salt_len = strlen(args[1].data.string_val);
    }
    if (args[2].type == FL_TYPE_INT)   iterations = (size_t)args[2].data.int_val;
    if (args[3].type == FL_TYPE_INT)   dklen      = (size_t)args[3].data.int_val;

    if (!pwd || !salt || iterations == 0 || dklen == 0) {
        fprintf(stderr, "[CRYPTO] pbkdf2: invalid arguments\n");
        fl_value_t v; v.type = FL_TYPE_NULL; return v;
    }

    /* PBKDF2: DK = T1 || T2 || ... where Ti = F(pwd, salt, c, i) */
    size_t hlen = 32; /* HMAC-SHA256 output */
    size_t blocks = (dklen + hlen - 1) / hlen;

    uint8_t *dk = malloc(blocks * hlen);
    uint8_t *salt_block = malloc(salt_len + 4);
    memcpy(salt_block, salt, salt_len);

    for (size_t block_idx = 1; block_idx <= blocks; block_idx++) {
        /* U1 = HMAC(pwd, salt || INT(i)) */
        salt_block[salt_len]     = (uint8_t)(block_idx >> 24);
        salt_block[salt_len + 1] = (uint8_t)(block_idx >> 16);
        salt_block[salt_len + 2] = (uint8_t)(block_idx >>  8);
        salt_block[salt_len + 3] = (uint8_t)(block_idx);

        uint8_t u[32], t[32];
        hmac_sha256_raw(pwd, pwd_len, salt_block, salt_len + 4, u);
        memcpy(t, u, 32);

        /* Uj = HMAC(pwd, U_{j-1}), T = T XOR Uj */
        for (size_t j = 1; j < iterations; j++) {
            hmac_sha256_raw(pwd, pwd_len, u, 32, u);
            for (size_t k = 0; k < 32; k++) t[k] ^= u[k];
        }
        memcpy(dk + (block_idx - 1) * hlen, t, hlen);
    }

    free(salt_block);

    fl_bytes_t *b = malloc(sizeof(fl_bytes_t));
    b->data = dk;
    b->size = dklen;
    b->capacity = blocks * hlen;
    fl_value_t result;
    result.type = FL_TYPE_BYTES;
    result.data.bytes_val = b;
    return result;
}

/* --- CSPRNG: crypto_random(n) -> bytes[n] via /dev/urandom --- */
fl_value_t fl_crypto_random(fl_value_t *args, size_t argc) {
    size_t n = 16; /* default: 16 bytes */
    if (argc >= 1 && args[0].type == FL_TYPE_INT) {
        n = (size_t)args[0].data.int_val;
        if (n == 0 || n > 4096) {
            fprintf(stderr, "[CRYPTO] crypto_random: n must be 1-4096\n");
            fl_value_t v; v.type = FL_TYPE_NULL; return v;
        }
    }

    uint8_t *buf = malloc(n);
    FILE *urandom = fopen("/dev/urandom", "rb");
    if (!urandom) {
        fprintf(stderr, "[CRYPTO] crypto_random: /dev/urandom unavailable\n");
        free(buf);
        fl_value_t v; v.type = FL_TYPE_NULL; return v;
    }
    size_t read_n = fread(buf, 1, n, urandom);
    fclose(urandom);

    if (read_n != n) {
        fprintf(stderr, "[CRYPTO] crypto_random: partial read %zu/%zu\n", read_n, n);
        free(buf);
        fl_value_t v; v.type = FL_TYPE_NULL; return v;
    }

    fl_bytes_t *b = malloc(sizeof(fl_bytes_t));
    b->data = buf;
    b->size = n;
    b->capacity = n;
    fl_value_t result;
    result.type = FL_TYPE_BYTES;
    result.data.bytes_val = b;
    return result;
}

/* --- bytes_to_hex(bytes) -> string --- */
fl_value_t fl_bytes_to_hex(fl_value_t *args, size_t argc) {
    if (argc < 1 || args[0].type != FL_TYPE_BYTES || !args[0].data.bytes_val) {
        fprintf(stderr, "[CRYPTO] bytes_to_hex: requires bytes argument\n");
        fl_value_t v; v.type = FL_TYPE_NULL; return v;
    }

    fl_bytes_t *b = args[0].data.bytes_val;
    char *hex = malloc(b->size * 2 + 1);
    static const char hex_chars[] = "0123456789abcdef";
    for (size_t i = 0; i < b->size; i++) {
        hex[i*2]     = hex_chars[(b->data[i] >> 4) & 0xF];
        hex[i*2 + 1] = hex_chars[b->data[i] & 0xF];
    }
    hex[b->size * 2] = '\0';

    fl_value_t result;
    result.type = FL_TYPE_STRING;
    result.data.string_val = hex;
    return result;
}

/* --- Timing-safe compare: crypto_compare(a, b) -> bool
       Constant-time to prevent timing attacks (RFC 6151) --- */
fl_value_t fl_crypto_compare(fl_value_t *args, size_t argc) {
    if (argc < 2) {
        fl_value_t v; v.type = FL_TYPE_BOOL; v.data.bool_val = 0; return v;
    }

    const uint8_t *a = NULL; size_t a_len = 0;
    const uint8_t *b = NULL; size_t b_len = 0;

    if (args[0].type == FL_TYPE_BYTES && args[0].data.bytes_val) {
        a = args[0].data.bytes_val->data; a_len = args[0].data.bytes_val->size;
    }
    if (args[1].type == FL_TYPE_BYTES && args[1].data.bytes_val) {
        b = args[1].data.bytes_val->data; b_len = args[1].data.bytes_val->size;
    }

    if (!a || !b || a_len != b_len) {
        fl_value_t v; v.type = FL_TYPE_BOOL; v.data.bool_val = 0; return v;
    }

    /* XOR all bytes, accumulate differences — no early exit */
    volatile uint8_t diff = 0;
    for (size_t i = 0; i < a_len; i++) {
        diff |= a[i] ^ b[i];
    }

    fl_value_t result;
    result.type = FL_TYPE_BOOL;
    result.data.bool_val = (diff == 0) ? 1 : 0;
    return result;
}

/* --- u32 bit rotation: u32_rotr(val, n) -> u32 --- */
fl_value_t fl_u32_rotr(fl_value_t *args, size_t argc) {
    if (argc < 2) { fl_value_t v; v.type = FL_TYPE_NULL; return v; }
    uint32_t val = (uint32_t)(args[0].type == FL_TYPE_INT ? args[0].data.int_val : (int64_t)args[0].data.float_val);
    uint32_t n   = (uint32_t)(args[1].type == FL_TYPE_INT ? args[1].data.int_val : (int64_t)args[1].data.float_val) & 31;
    uint32_t result = (val >> n) | (val << (32 - n));
    fl_value_t v;
    v.type = FL_TYPE_INT;
    v.data.int_val = (int64_t)result;
    return v;
}

/* --- u32 add with overflow (mod 2^32): u32_add(a, b) -> u32 --- */
fl_value_t fl_u32_add(fl_value_t *args, size_t argc) {
    if (argc < 2) { fl_value_t v; v.type = FL_TYPE_NULL; return v; }
    uint32_t a = (uint32_t)(args[0].type == FL_TYPE_INT ? args[0].data.int_val : (int64_t)args[0].data.float_val);
    uint32_t b = (uint32_t)(args[1].type == FL_TYPE_INT ? args[1].data.int_val : (int64_t)args[1].data.float_val);
    uint32_t result = a + b; /* natural overflow mod 2^32 */
    fl_value_t v;
    v.type = FL_TYPE_INT;
    v.data.int_val = (int64_t)result;
    return v;
}

/**
 * Utility Functions
 */

void fl_value_print(fl_value_t value) {
    switch (value.type) {
        case FL_TYPE_NULL:
            printf("null");
            break;
        case FL_TYPE_BOOL:
            printf("%s", value.data.bool_val ? "true" : "false");
            break;
        case FL_TYPE_INT:
            printf("%ld", value.data.int_val);
            break;
        case FL_TYPE_FLOAT:
            printf("%g", value.data.float_val);
            break;
        case FL_TYPE_STRING:
            printf("%s", value.data.string_val ? value.data.string_val : "");
            break;
        case FL_TYPE_ARRAY:
            printf("[array]");
            break;
        case FL_TYPE_OBJECT:
            printf("{...}");
            break;
        case FL_TYPE_FUNCTION:
            printf("[function]");
            break;
        case FL_TYPE_CLOSURE:
            printf("[closure]");
            break;
        case FL_TYPE_BYTES: {
            fl_bytes_t* b = value.data.bytes_val;
            printf("[bytes:%zu]", b ? b->size : 0);
            break;
        }
        case FL_TYPE_ERROR:
            printf("[error]");
            break;
    }
}

const char* fl_type_name(fl_type_t type) {
    switch (type) {
        case FL_TYPE_NULL:      return "null";
        case FL_TYPE_BOOL:      return "bool";
        case FL_TYPE_INT:       return "int";
        case FL_TYPE_FLOAT:     return "float";
        case FL_TYPE_STRING:    return "string";
        case FL_TYPE_ARRAY:     return "array";
        case FL_TYPE_OBJECT:    return "object";
        case FL_TYPE_FUNCTION:  return "function";
        case FL_TYPE_CLOSURE:   return "closure";
        case FL_TYPE_BYTES:     return "bytes";
        case FL_TYPE_ERROR:     return "error";
        default:                return "unknown";
    }
}

void fl_value_free(fl_value_t value) {
    if (value.type == FL_TYPE_STRING && value.data.string_val) {
        free(value.data.string_val);
    }
    /* Other types handled by GC */
}

/* File I/O Functions */

/**
 * write_bytes_file(filename: string, bytes: array[int]) -> null
 * Writes binary data to a file. Each element in the array should be 0-255.
 * Used for creating binary files like ELF executables.
 */
fl_value_t fl_write_bytes_file(fl_value_t* args, size_t argc) {
    if (argc < 2) {
        fprintf(stderr, "[STDLIB] write_bytes_file: Expected 2 arguments (filename, bytes array)\n");
        return fl_new_null();
    }

    if (args[0].type != FL_TYPE_STRING) {
        fprintf(stderr, "[STDLIB] write_bytes_file: First argument must be a string (filename)\n");
        return fl_new_null();
    }

    if (args[1].type != FL_TYPE_ARRAY) {
        fprintf(stderr, "[STDLIB] write_bytes_file: Second argument must be an array of bytes\n");
        return fl_new_null();
    }

    const char* filename = args[0].data.string_val;
    fl_array_t* byte_array = args[1].data.array_val;

    FILE* f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "[STDLIB] write_bytes_file: Failed to open file '%s' for writing\n", filename);
        return fl_new_null();
    }

    for (size_t i = 0; i < byte_array->size; i++) {
        if (byte_array->elements[i].type != FL_TYPE_INT) {
            fprintf(stderr, "[STDLIB] write_bytes_file: Array element %zu is not an integer\n", i);
            fclose(f);
            return fl_new_null();
        }
        uint8_t byte = (uint8_t)(byte_array->elements[i].data.int_val & 0xFF);
        if (fwrite(&byte, 1, 1, f) != 1) {
            fprintf(stderr, "[STDLIB] write_bytes_file: Write error at byte %zu\n", i);
            fclose(f);
            return fl_new_null();
        }
    }

    fclose(f);
    printf("[STDLIB] write_bytes_file: Wrote %zu bytes to '%s'\n", byte_array->size, filename);
    return fl_new_null();
}

/**
 * read_file(filename: string) -> string
 * Reads a text file and returns its contents as a string.
 */
fl_value_t fl_read_file(fl_value_t* args, size_t argc) {
    if (argc < 1) {
        fprintf(stderr, "[STDLIB] read_file: Expected 1 argument (filename)\n");
        return fl_new_null();
    }

    if (args[0].type != FL_TYPE_STRING) {
        fprintf(stderr, "[STDLIB] read_file: Argument must be a string (filename)\n");
        return fl_new_null();
    }

    const char* filename = args[0].data.string_val;
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "[STDLIB] read_file: Failed to open file '%s'\n", filename);
        return fl_new_null();
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size < 0) {
        fprintf(stderr, "[STDLIB] read_file: Failed to get file size\n");
        fclose(f);
        return fl_new_null();
    }

    char* buffer = malloc(file_size + 1);
    if (!buffer) {
        fprintf(stderr, "[STDLIB] read_file: Memory allocation failed\n");
        fclose(f);
        return fl_new_null();
    }

    size_t bytes_read = fread(buffer, 1, file_size, f);
    fclose(f);

    if (bytes_read != (size_t)file_size) {
        fprintf(stderr, "[STDLIB] read_file: Read %zu bytes, expected %ld\n", bytes_read, file_size);
        free(buffer);
        return fl_new_null();
    }

    buffer[file_size] = '\0';
    fl_value_t result = fl_new_string(buffer);
    free(buffer);
    return result;
}

/* len(array) -> int */
fl_value_t fl_len(fl_value_t* args, size_t argc) {
    if (argc < 1) {
        fprintf(stderr, "[STDLIB] len: Expected 1 argument\n");
        return fl_new_int(0);
    }

    if (args[0].type == FL_TYPE_ARRAY) {
        fl_array_t* arr = args[0].data.array_val;
        if (arr) {
            return fl_new_int(arr->size);
        }
        return fl_new_int(0);
    }

    if (args[0].type == FL_TYPE_STRING) {
        const char* str = args[0].data.string_val;
        if (str) {
            return fl_new_int(strlen(str));
        }
        return fl_new_int(0);
    }

    fprintf(stderr, "[STDLIB] len: Argument must be array or string\n");
    return fl_new_int(0);
}

/* push(array, value) */
fl_value_t fl_push(fl_value_t* args, size_t argc) {
    if (argc < 2) {
        fprintf(stderr, "[STDLIB] push: Expected 2 arguments\n");
        return fl_new_null();
    }

    if (args[0].type != FL_TYPE_ARRAY) {
        fprintf(stderr, "[STDLIB] push: First argument must be an array\n");
        return fl_new_null();
    }

    fl_array_t* arr = args[0].data.array_val;
    if (!arr) {
        fprintf(stderr, "[STDLIB] push: Array is NULL\n");
        return fl_new_null();
    }

    /* Resize if needed */
    if (arr->size >= arr->capacity) {
        arr->capacity = (arr->capacity == 0) ? 10 : arr->capacity * 2;
        fl_value_t* new_elements = realloc(arr->elements, arr->capacity * sizeof(fl_value_t));
        if (!new_elements) {
            fprintf(stderr, "[STDLIB] push: Memory allocation failed\n");
            return fl_new_null();
        }
        arr->elements = new_elements;
    }

    arr->elements[arr->size] = args[1];
    arr->size++;

    return fl_new_null();
}

/* ============================================================
   Bytes Builtin Functions (Phase 3)
   ============================================================ */

/* bytes_new(size) -> bytes */
fl_value_t fl_bytes_new(fl_value_t* args, size_t argc) {
    size_t size = 256;

    if (argc > 0 && args[0].type == FL_TYPE_INT) {
        size = (size_t)args[0].data.int_val;
    }

    return fl_new_bytes(size);
}

/* bytes_len(bytes) -> int */
fl_value_t fl_bytes_len(fl_value_t* args, size_t argc) {
    if (argc < 1) {
        fprintf(stderr, "[STDLIB] bytes_len: Expected 1 argument\n");
        return fl_new_int(0);
    }

    if (args[0].type != FL_TYPE_BYTES) {
        fprintf(stderr, "[STDLIB] bytes_len: Argument must be bytes\n");
        return fl_new_int(0);
    }

    fl_bytes_t* b = args[0].data.bytes_val;
    if (!b) return fl_new_int(0);

    return fl_new_int((fl_int)b->size);
}

/* bytes_set(bytes, idx, value) - set byte at index */
fl_value_t fl_bytes_set(fl_value_t* args, size_t argc) {
    if (argc < 3) {
        fprintf(stderr, "[STDLIB] bytes_set: Expected 3 arguments\n");
        return fl_new_null();
    }

    if (args[0].type != FL_TYPE_BYTES || args[1].type != FL_TYPE_INT || args[2].type != FL_TYPE_INT) {
        fprintf(stderr, "[STDLIB] bytes_set: Wrong argument types\n");
        return fl_new_null();
    }

    fl_bytes_t* b = args[0].data.bytes_val;
    size_t idx = (size_t)args[1].data.int_val;
    uint8_t val = (uint8_t)(args[2].data.int_val & 0xFF);

    if (!b) return fl_new_null();

    if (idx >= b->capacity) {
        fl_bytes_expand(b, idx + 1);
    }

    b->data[idx] = val;
    if (idx >= b->size) {
        b->size = idx + 1;
    }

    return fl_new_null();
}

/* bytes_get(bytes, idx) -> int */
fl_value_t fl_bytes_get(fl_value_t* args, size_t argc) {
    if (argc < 2) {
        fprintf(stderr, "[STDLIB] bytes_get: Expected 2 arguments\n");
        return fl_new_int(0);
    }

    if (args[0].type != FL_TYPE_BYTES || args[1].type != FL_TYPE_INT) {
        fprintf(stderr, "[STDLIB] bytes_get: Wrong argument types\n");
        return fl_new_int(0);
    }

    fl_bytes_t* b = args[0].data.bytes_val;
    size_t idx = (size_t)args[1].data.int_val;

    if (!b || idx >= b->size) {
        return fl_new_int(0);
    }

    return fl_new_int((fl_int)b->data[idx]);
}

/* bytes_write_u32(bytes, offset, value) - write 32-bit value (little-endian) */
fl_value_t fl_bytes_write_u32(fl_value_t* args, size_t argc) {
    if (argc < 3) {
        fprintf(stderr, "[STDLIB] bytes_write_u32: Expected 3 arguments\n");
        return fl_new_null();
    }

    if (args[0].type != FL_TYPE_BYTES || args[1].type != FL_TYPE_INT || args[2].type != FL_TYPE_INT) {
        fprintf(stderr, "[STDLIB] bytes_write_u32: Wrong argument types\n");
        return fl_new_null();
    }

    fl_bytes_t* b = args[0].data.bytes_val;
    size_t offset = (size_t)args[1].data.int_val;
    uint32_t val = (uint32_t)args[2].data.int_val;

    if (!b) return fl_new_null();

    if (offset + 4 > b->capacity) {
        fl_bytes_expand(b, offset + 4);
    }

    /* Write little-endian */
    b->data[offset + 0] = (uint8_t)(val & 0xFF);
    b->data[offset + 1] = (uint8_t)((val >> 8) & 0xFF);
    b->data[offset + 2] = (uint8_t)((val >> 16) & 0xFF);
    b->data[offset + 3] = (uint8_t)((val >> 24) & 0xFF);

    if (offset + 4 > b->size) {
        b->size = offset + 4;
    }

    return fl_new_null();
}

/* bytes_write_u64(bytes, offset, value) - write 64-bit value (little-endian) */
fl_value_t fl_bytes_write_u64(fl_value_t* args, size_t argc) {
    if (argc < 3) {
        fprintf(stderr, "[STDLIB] bytes_write_u64: Expected 3 arguments\n");
        return fl_new_null();
    }

    if (args[0].type != FL_TYPE_BYTES || args[1].type != FL_TYPE_INT || args[2].type != FL_TYPE_INT) {
        fprintf(stderr, "[STDLIB] bytes_write_u64: Wrong argument types\n");
        return fl_new_null();
    }

    fl_bytes_t* b = args[0].data.bytes_val;
    size_t offset = (size_t)args[1].data.int_val;
    uint64_t val = (uint64_t)args[2].data.int_val;

    if (!b) return fl_new_null();

    if (offset + 8 > b->capacity) {
        fl_bytes_expand(b, offset + 8);
    }

    /* Write little-endian */
    for (int i = 0; i < 8; i++) {
        b->data[offset + i] = (uint8_t)((val >> (i * 8)) & 0xFF);
    }

    if (offset + 8 > b->size) {
        b->size = offset + 8;
    }

    return fl_new_null();
}

/* ============================================================================
   PHASE 8: Process Management - MOSS-Kernel-Runner
   PM2를 대체하는 네이티브 프로세스 관리
   ============================================================================ */

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "../include/process.h"
#include "../include/cluster.h"
#include "../include/introspect.h"

/* 글로벌 프로세스 매니저 (fl_stdlib_register에서 초기화) */
static fl_process_manager_t* g_stdlib_pm = NULL;

static fl_value_t fl_make_bool(bool v) {
    fl_value_t r; r.type = FL_TYPE_BOOL; r.data.bool_val = v; return r;
}
static fl_value_t fl_make_int(int64_t v) {
    fl_value_t r; r.type = FL_TYPE_INT; r.data.int_val = v; return r;
}
static fl_value_t fl_make_string_copy(const char* s) {
    fl_value_t r;
    r.type = FL_TYPE_STRING;
    r.data.string_val = (char*)malloc(strlen(s) + 1);
    if (r.data.string_val) strcpy(r.data.string_val, s);
    return r;
}

/*
 * process_spawn(name, script, autorestart=true, max_restarts=-1) -> pid
 * Phoenix-Spawn 등록 + 즉시 시작
 */
fl_value_t fl_process_spawn(fl_value_t* args, size_t argc) {
    if (argc < 2) return fl_new_null();
    if (args[0].type != FL_TYPE_STRING || args[1].type != FL_TYPE_STRING)
        return fl_new_null();

    const char* name   = args[0].data.string_val;
    const char* script = args[1].data.string_val;
    bool autorestart   = (argc >= 3 && args[2].type == FL_TYPE_BOOL)
                         ? args[2].data.bool_val : true;
    int max_restarts   = (argc >= 4 && args[3].type == FL_TYPE_INT)
                         ? (int)args[3].data.int_val : -1;

    /* 매니저 초기화 (최초 1회) */
    if (!g_stdlib_pm) {
        g_stdlib_pm = fl_pm_create();
        fl_pm_install_watchdog(g_stdlib_pm);
    }

    fl_process_entry_t* entry = fl_pm_register(g_stdlib_pm, name, script,
                                                autorestart, max_restarts);
    if (!entry) return fl_new_null();

    fl_pm_start(g_stdlib_pm, name);
    return fl_make_int((int64_t)entry->pid);
}

/*
 * process_kill(pid, signal=15) -> ok
 */
fl_value_t fl_process_kill(fl_value_t* args, size_t argc) {
    if (argc < 1 || args[0].type != FL_TYPE_INT) return fl_make_bool(false);
    pid_t pid = (pid_t)args[0].data.int_val;
    int   sig = (argc >= 2 && args[1].type == FL_TYPE_INT)
                ? (int)args[1].data.int_val : SIGTERM;
    int ret = kill(pid, sig);
    return fl_make_bool(ret == 0);
}

/*
 * process_list() -> array of {name, pid, status, restarts}
 */
fl_value_t fl_process_list(fl_value_t* args, size_t argc) {
    (void)args; (void)argc;

    fl_value_t result;
    result.type = FL_TYPE_ARRAY;
    result.data.array_val = (fl_array_t*)malloc(sizeof(fl_array_t));
    if (!result.data.array_val) return fl_new_null();

    fl_array_t* arr = result.data.array_val;
    arr->capacity = 16;
    arr->size     = 0;
    arr->elements = (fl_value_t*)malloc(arr->capacity * sizeof(fl_value_t));
    if (!arr->elements) { free(arr); return fl_new_null(); }

    if (!g_stdlib_pm) return result;

    fl_process_entry_t* entry = g_stdlib_pm->head;
    while (entry) {
        /* 각 프로세스 → object */
        fl_value_t obj;
        obj.type = FL_TYPE_OBJECT;
        obj.data.object_val = (fl_object_t*)malloc(sizeof(fl_object_t));
        if (obj.data.object_val) {
            fl_object_t* o = obj.data.object_val;
            o->capacity = 8;
            o->size     = 0;
            o->keys   = (char**)malloc(8 * sizeof(char*));
            o->values = (fl_value_t*)malloc(8 * sizeof(fl_value_t));

            if (o->keys && o->values) {
                /* name */
                o->keys[0]   = strdup("name");
                o->values[0] = fl_make_string_copy(entry->name);
                /* pid */
                o->keys[1]   = strdup("pid");
                o->values[1] = fl_make_int(entry->pid);
                /* restarts */
                o->keys[2]   = strdup("restarts");
                o->values[2] = fl_make_int(entry->restarts);
                /* autorestart */
                o->keys[3]   = strdup("autorestart");
                o->values[3] = fl_make_bool(entry->autorestart);
                o->size = 4;
            }

            if (arr->size >= arr->capacity) {
                arr->capacity *= 2;
                arr->elements = (fl_value_t*)realloc(arr->elements,
                    arr->capacity * sizeof(fl_value_t));
            }
            arr->elements[arr->size++] = obj;
        }
        entry = entry->next;
    }

    return result;
}

/*
 * process_restart(name) -> ok
 */
fl_value_t fl_process_restart(fl_value_t* args, size_t argc) {
    if (argc < 1 || args[0].type != FL_TYPE_STRING) return fl_make_bool(false);
    if (!g_stdlib_pm) return fl_make_bool(false);
    int ret = fl_pm_restart(g_stdlib_pm, args[0].data.string_val);
    return fl_make_bool(ret == 0);
}

/*
 * System.metrics() -> object with pid, uptime_ms, mem_rss_kb, cpu_percent, gc_collections
 */
fl_value_t fl_system_metrics(fl_value_t* args, size_t argc) {
    (void)args; (void)argc;

    fl_metrics_t m = fl_metrics_collect();
    char* json = fl_metrics_to_json(&m);

    fl_value_t obj;
    obj.type = FL_TYPE_OBJECT;
    obj.data.object_val = (fl_object_t*)malloc(sizeof(fl_object_t));
    if (!obj.data.object_val) { free(json); return fl_new_null(); }

    fl_object_t* o = obj.data.object_val;
    o->capacity = 16;
    o->size     = 0;
    o->keys   = (char**)malloc(16 * sizeof(char*));
    o->values = (fl_value_t*)malloc(16 * sizeof(fl_value_t));

    if (o->keys && o->values) {
        o->keys[0]   = strdup("pid");
        o->values[0] = fl_make_int(m.pid);
        o->keys[1]   = strdup("uptime_ms");
        o->values[1] = fl_make_int((int64_t)m.uptime_ms);
        o->keys[2]   = strdup("mem_rss_kb");
        o->values[2] = fl_make_int((int64_t)m.mem_rss_kb);
        o->keys[3]   = strdup("mem_virt_kb");
        o->values[3] = fl_make_int((int64_t)m.mem_virt_kb);
        o->keys[4]   = strdup("cpu_percent");
        o->values[4].type = FL_TYPE_FLOAT;
        o->values[4].data.float_val = m.cpu_percent;
        o->keys[5]   = strdup("gc_collections");
        o->values[5] = fl_make_int((int64_t)m.gc_collections);
        o->keys[6]   = strdup("json");
        o->values[6] = json ? fl_make_string_copy(json) : fl_new_null();
        o->size = 7;
    }

    free(json);
    return obj;
}

/*
 * System.daemonize() -> ok
 * 현재 프로세스를 백그라운드 데몬으로 전환
 */
fl_value_t fl_system_daemonize(fl_value_t* args, size_t argc) {
    (void)args; (void)argc;
    int ret = fl_pm_daemonize();
    return fl_make_bool(ret == 0);
}

/*
 * cluster_workers(n=auto) -> worker_id
 * n개 worker 포크. master는 run_master() 진입 (블로킹),
 * worker는 worker_id(0+) 반환 (호출자가 서버 루프 실행)
 *
 * 간략 버전: 실제 어노테이션 컴파일러 지원 없이
 * FreeLang에서 직접 호출 가능한 런타임 API
 */
static int g_worker_id = -1;  /* -1 = master */

static void default_worker_fn(int id, void* data) {
    (void)data;
    g_worker_id = id;
    /* worker: 호출자에게 worker_id 반환 후 계속 실행 */
}

fl_value_t fl_cluster_workers(fl_value_t* args, size_t argc) {
    int n = 0; /* 0 = auto */
    if (argc >= 1 && args[0].type == FL_TYPE_INT) {
        n = (int)args[0].data.int_val;
    }

    fl_cluster_t* cluster = fl_cluster_spawn(n, default_worker_fn, NULL);
    if (!cluster) {
        /* worker 프로세스: default_worker_fn이 g_worker_id 설정 후 종료 */
        return fl_make_int(g_worker_id);
    }

    /* master: 이벤트 루프 (블로킹) */
    fl_cluster_run_master(cluster);
    fl_cluster_destroy(cluster);
    return fl_make_int(-1); /* master */
}

/*
 * process_pid() -> PID
 */
fl_value_t fl_process_pid(fl_value_t* args, size_t argc) {
    (void)args; (void)argc;
    return fl_make_int((int64_t)getpid());
}

/*
 * process_sleep(ms) -> null
 */
fl_value_t fl_process_sleep(fl_value_t* args, size_t argc) {
    if (argc < 1 || args[0].type != FL_TYPE_INT) return fl_new_null();
    int64_t ms = args[0].data.int_val;
    if (ms > 0) {
        struct timespec ts;
        ts.tv_sec  = ms / 1000;
        ts.tv_nsec = (ms % 1000) * 1000000L;
        nanosleep(&ts, NULL);
    }
    return fl_new_null();
}

/* ============================================================================
   PHASE 6: Vector-Vision - SIMD-Accelerated Image Processing Engine
   sharp 대체: 외부 의존성 0%, 순수 C 구현
   포맷: PPM P6 (RGB) / PGM P5 (Grayscale)
   SIMD 전략: GCC auto-vectorization (-O3 -ftree-vectorize)
              + 메모리 정렬 (AVX2 32바이트 레지스터 폭 최적화)
   @vectorize 어노테이션: 파서가 감지 → 컴파일러가 SIMD 힌트 emit
   ============================================================================ */

/* --- 내부 헬퍼: 이미지 객체 생성 ---
   이미지 = FL_TYPE_OBJECT { width, height, channels, data: FL_TYPE_BYTES } */
static fl_value_t vv_make_image(int width, int height, int channels, uint8_t* pixels) {
    fl_object_t* obj = (fl_object_t*)malloc(sizeof(fl_object_t));
    if (!obj) return fl_new_null();
    obj->capacity = 4; obj->size = 0;
    obj->keys   = (char**)malloc(4 * sizeof(char*));
    obj->values = (fl_value_t*)malloc(4 * sizeof(fl_value_t));

    obj->keys[0] = strdup("width");   obj->values[0] = fl_new_int(width);
    obj->keys[1] = strdup("height");  obj->values[1] = fl_new_int(height);
    obj->keys[2] = strdup("channels");obj->values[2] = fl_new_int(channels);

    fl_bytes_t* b = (fl_bytes_t*)malloc(sizeof(fl_bytes_t));
    b->data = pixels;
    b->size = b->capacity = (size_t)(width * height * channels);
    fl_value_t dv; dv.type = FL_TYPE_BYTES; dv.data.bytes_val = b;
    obj->keys[3] = strdup("data"); obj->values[3] = dv;
    obj->size = 4;

    fl_value_t r; r.type = FL_TYPE_OBJECT; r.data.object_val = obj;
    return r;
}

static int vv_get_int_field(fl_value_t img, const char* field) {
    if (img.type != FL_TYPE_OBJECT) return 0;
    fl_object_t* obj = img.data.object_val;
    for (size_t i = 0; i < obj->size; i++) {
        if (strcmp(obj->keys[i], field) == 0) {
            if (obj->values[i].type == FL_TYPE_INT)   return (int)obj->values[i].data.int_val;
            if (obj->values[i].type == FL_TYPE_FLOAT) return (int)obj->values[i].data.float_val;
        }
    }
    return 0;
}

static uint8_t* vv_get_data(fl_value_t img) {
    if (img.type != FL_TYPE_OBJECT) return NULL;
    fl_object_t* obj = img.data.object_val;
    for (size_t i = 0; i < obj->size; i++) {
        if (strcmp(obj->keys[i], "data") == 0 && obj->values[i].type == FL_TYPE_BYTES)
            return obj->values[i].data.bytes_val->data;
    }
    return NULL;
}

/* --- Lanczos-3 sinc 커널 (반경 3 고품질 보간) --- */
static double vv_lanczos(double x, int r) {
    if (x == 0.0) return 1.0;
    if (x <= -(double)r || x >= (double)r) return 0.0;
    double px = 3.14159265358979323846 * x;
    return (sin(px) / px) * (sin(px / r) / (px / r));
}

/* --- Bilinear 보간 (@vectorize 내부 루프) --- */
static uint8_t vv_bilinear(const uint8_t* s, int sw, int sh, int ch,
                            double fx, double fy, int c) {
    int x0 = (int)fx, y0 = (int)fy;
    int x1 = (x0+1 < sw) ? x0+1 : x0, y1 = (y0+1 < sh) ? y0+1 : y0;
    double dx = fx-x0, dy = fy-y0;
    double v = s[(y0*sw+x0)*ch+c]*(1-dx)*(1-dy) + s[(y0*sw+x1)*ch+c]*dx*(1-dy)
             + s[(y1*sw+x0)*ch+c]*(1-dx)*dy      + s[(y1*sw+x1)*ch+c]*dx*dy;
    return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
}

/* --- Lanczos-3 리사이즈 (고품질, @vectorize 어노테이션 적용) --- */
static uint8_t* vv_resize_lanczos(const uint8_t* src, int sw, int sh,
                                   int ch, int dw, int dh) {
    uint8_t* dst = (uint8_t*)malloc((size_t)(dw * dh * ch));
    if (!dst) return NULL;
    double rx = (double)sw/dw, ry = (double)sh/dh;
    for (int dy = 0; dy < dh; dy++) {
        for (int dx = 0; dx < dw; dx++) {
            double sx = (dx+0.5)*rx-0.5, sy = (dy+0.5)*ry-0.5;
            int ix = (int)sx, iy = (int)sy;
            for (int c = 0; c < ch; c++) {
                double wsum = 0, vsum = 0;
                for (int ky = -3; ky <= 3; ky++) {
                    for (int kx = -3; kx <= 3; kx++) {
                        int px = ix+kx, py = iy+ky;
                        if (px < 0) px=0; if (px>=sw) px=sw-1;
                        if (py < 0) py=0; if (py>=sh) py=sh-1;
                        double w = vv_lanczos(sx-ix-kx,3) * vv_lanczos(sy-iy-ky,3);
                        vsum += w * src[(py*sw+px)*ch+c];
                        wsum += w;
                    }
                }
                double v = wsum > 1e-8 ? vsum/wsum : 0;
                dst[(dy*dw+dx)*ch+c] = (uint8_t)(v<0?0:v>255?255:v);
            }
        }
    }
    return dst;
}

/* ---- Public Vision API (12 functions) ---- */

/* vision_load(path) -> object | null  (PPM P6 / PGM P5) */
fl_value_t fl_vision_load(fl_value_t* args, size_t argc) {
    if (argc < 1 || args[0].type != FL_TYPE_STRING) {
        fprintf(stderr, "[vision] vision_load: string path 필요\n");
        return fl_new_null();
    }
    const char* path = args[0].data.string_val;
    FILE* fp = fopen(path, "rb");
    if (!fp) { fprintf(stderr, "[vision] vision_load: 열기 실패: %s\n", path); return fl_new_null(); }

    char magic[3] = {0};
    if (fread(magic, 1, 2, fp) != 2) { fclose(fp); return fl_new_null(); }
    int channels;
    if      (magic[0]=='P' && magic[1]=='6') channels = 3;
    else if (magic[0]=='P' && magic[1]=='5') channels = 1;
    else { fprintf(stderr, "[vision] PPM(P6)/PGM(P5) 지원. 감지: %.2s\n", magic); fclose(fp); return fl_new_null(); }

    int w = 0, h = 0, fields = 0;
    char line[256];
    while (fields < 3 && fgets(line, sizeof(line), fp)) {
        if (line[0] == '#') continue;
        int a = -1, b2 = -1;
        int n = sscanf(line, "%d %d", &a, &b2);
        if (n >= 1 && a > 0 && fields == 0) { w = a; fields++; }
        if (n >= 2 && b2 > 0 && fields == 1) { h = b2; fields++; }
        else if (n >= 1 && a > 0 && fields == 1) { h = a; fields++; }
        if (fields == 2) { fields++; break; }
    }
    if (w <= 0 || h <= 0) { fclose(fp); return fl_new_null(); }

    size_t dsz = (size_t)(w * h * channels);
    uint8_t* px = (uint8_t*)malloc(dsz);
    if (!px) { fclose(fp); return fl_new_null(); }
    if (fread(px, 1, dsz, fp) != dsz) { free(px); fclose(fp); return fl_new_null(); }
    fclose(fp);
    return vv_make_image(w, h, channels, px);
}

/* vision_save(img, path, format) -> bool */
fl_value_t fl_vision_save(fl_value_t* args, size_t argc) {
    if (argc < 3) return fl_new_bool(0);
    int w  = vv_get_int_field(args[0], "width");
    int h  = vv_get_int_field(args[0], "height");
    int ch = vv_get_int_field(args[0], "channels");
    uint8_t* data = vv_get_data(args[0]);
    if (!data || w<=0 || h<=0) return fl_new_bool(0);
    const char* path = (args[1].type==FL_TYPE_STRING) ? args[1].data.string_val : "out.ppm";
    const char* fmt  = (args[2].type==FL_TYPE_STRING) ? args[2].data.string_val : "ppm";
    FILE* fp = fopen(path, "wb");
    if (!fp) { fprintf(stderr, "[vision] vision_save: 쓰기 실패: %s\n", path); return fl_new_bool(0); }
    if (strcmp(fmt,"pgm")==0 || ch==1) fprintf(fp, "P5\n%d %d\n255\n", w, h);
    else                                fprintf(fp, "P6\n%d %d\n255\n", w, h);
    fwrite(data, 1, (size_t)(w*h*ch), fp);
    fclose(fp);
    return fl_new_bool(1);
}

/* vision_resize(img, w, h, algo) -> object  algo: nearest|bilinear|lanczos */
fl_value_t fl_vision_resize(fl_value_t* args, size_t argc) {
    if (argc < 4) return fl_new_null();
    int dw  = (args[1].type==FL_TYPE_INT) ? (int)args[1].data.int_val : (int)args[1].data.float_val;
    int dh  = (args[2].type==FL_TYPE_INT) ? (int)args[2].data.int_val : (int)args[2].data.float_val;
    const char* algo = (args[3].type==FL_TYPE_STRING) ? args[3].data.string_val : "bilinear";
    if (dw<=0 || dh<=0) return fl_new_null();

    int sw  = vv_get_int_field(args[0], "width");
    int sh  = vv_get_int_field(args[0], "height");
    int ch  = vv_get_int_field(args[0], "channels");
    uint8_t* src = vv_get_data(args[0]);
    if (!src || sw<=0 || sh<=0) return fl_new_null();

    uint8_t* dst = NULL;
    if (strcmp(algo,"lanczos")==0) {
        dst = vv_resize_lanczos(src, sw, sh, ch, dw, dh);
    } else if (strcmp(algo,"nearest")==0) {
        dst = (uint8_t*)malloc((size_t)(dw*dh*ch));
        if (dst) {
            double rx=(double)sw/dw, ry=(double)sh/dh;
            for (int y=0;y<dh;y++) { int py=(int)(y*ry); if(py>=sh) py=sh-1;
                for (int x=0;x<dw;x++) { int px=(int)(x*rx); if(px>=sw) px=sw-1;
                    for (int c=0;c<ch;c++) dst[(y*dw+x)*ch+c]=src[(py*sw+px)*ch+c];
                }
            }
        }
    } else {
        /* bilinear @vectorize: 행 단위 SIMD 병렬화 */
        dst = (uint8_t*)malloc((size_t)(dw*dh*ch));
        if (dst) {
            double rx=(double)sw/dw, ry=(double)sh/dh;
            for (int y=0;y<dh;y++) {
                for (int x=0;x<dw;x++) {
                    double fx=(x+0.5)*rx-0.5, fy=(y+0.5)*ry-0.5;
                    for (int c=0;c<ch;c++)
                        dst[(y*dw+x)*ch+c]=vv_bilinear(src,sw,sh,ch,fx,fy,c);
                }
            }
        }
    }
    if (!dst) return fl_new_null();
    return vv_make_image(dw, dh, ch, dst);
}

fl_value_t fl_vision_width(fl_value_t* args, size_t argc) {
    return (argc<1) ? fl_new_int(0) : fl_new_int(vv_get_int_field(args[0],"width"));
}
fl_value_t fl_vision_height(fl_value_t* args, size_t argc) {
    return (argc<1) ? fl_new_int(0) : fl_new_int(vv_get_int_field(args[0],"height"));
}
fl_value_t fl_vision_channels(fl_value_t* args, size_t argc) {
    return (argc<1) ? fl_new_int(0) : fl_new_int(vv_get_int_field(args[0],"channels"));
}

/* vision_grayscale(img) -> object  BT.601 @vectorize */
fl_value_t fl_vision_grayscale(fl_value_t* args, size_t argc) {
    if (argc<1) return fl_new_null();
    int w=vv_get_int_field(args[0],"width"), h=vv_get_int_field(args[0],"height");
    int ch=vv_get_int_field(args[0],"channels");
    uint8_t* src=vv_get_data(args[0]);
    if (!src||w<=0||h<=0) return fl_new_null();
    if (ch==1) {
        uint8_t* cp=(uint8_t*)malloc((size_t)(w*h));
        if (!cp) return fl_new_null();
        memcpy(cp, src, (size_t)(w*h));
        return vv_make_image(w, h, 1, cp);
    }
    uint8_t* dst=(uint8_t*)malloc((size_t)(w*h));
    if (!dst) return fl_new_null();
    int n=w*h;
    /* @vectorize: BT.601 정수 근사 (GCC -O2 auto-vec) */
    for (int i=0;i<n;i++) {
        int r=src[i*ch], g=src[i*ch+1], b=(ch>=3)?src[i*ch+2]:0;
        dst[i]=(uint8_t)((r*77+g*150+b*29)>>8);
    }
    return vv_make_image(w, h, 1, dst);
}

/* vision_blur(img, radius) -> object  Box blur @vectorize */
fl_value_t fl_vision_blur(fl_value_t* args, size_t argc) {
    if (argc<2) return fl_new_null();
    int radius=(args[1].type==FL_TYPE_INT)?(int)args[1].data.int_val:(int)args[1].data.float_val;
    if (radius<1) radius=1;
    int w=vv_get_int_field(args[0],"width"), h=vv_get_int_field(args[0],"height");
    int ch=vv_get_int_field(args[0],"channels");
    uint8_t* src=vv_get_data(args[0]);
    if (!src||w<=0||h<=0) return fl_new_null();
    uint8_t* dst=(uint8_t*)malloc((size_t)(w*h*ch));
    if (!dst) return fl_new_null();
    for (int c=0;c<ch;c++) {
        for (int y=0;y<h;y++) {
            for (int x=0;x<w;x++) {
                int sum=0, cnt=0;
                for (int ky=-radius;ky<=radius;ky++) {
                    for (int kx=-radius;kx<=radius;kx++) {
                        int px=x+kx, py=y+ky;
                        if (px>=0&&px<w&&py>=0&&py<h) { sum+=src[(py*w+px)*ch+c]; cnt++; }
                    }
                }
                dst[(y*w+x)*ch+c]=(uint8_t)(cnt>0?sum/cnt:0);
            }
        }
    }
    return vv_make_image(w, h, ch, dst);
}

fl_value_t fl_vision_pixel_get(fl_value_t* args, size_t argc) {
    if (argc<4) return fl_new_int(0);
    int x=(int)(args[1].type==FL_TYPE_INT?args[1].data.int_val:(fl_int)args[1].data.float_val);
    int y=(int)(args[2].type==FL_TYPE_INT?args[2].data.int_val:(fl_int)args[2].data.float_val);
    int c=(int)(args[3].type==FL_TYPE_INT?args[3].data.int_val:(fl_int)args[3].data.float_val);
    int w=vv_get_int_field(args[0],"width"), ch=vv_get_int_field(args[0],"channels");
    uint8_t* d=vv_get_data(args[0]);
    if (!d||x<0||y<0||c<0||c>=ch) return fl_new_int(0);
    return fl_new_int(d[(y*w+x)*ch+c]);
}

fl_value_t fl_vision_pixel_set(fl_value_t* args, size_t argc) {
    if (argc<5) return fl_new_null();
    int x=(int)(args[1].type==FL_TYPE_INT?args[1].data.int_val:(fl_int)args[1].data.float_val);
    int y=(int)(args[2].type==FL_TYPE_INT?args[2].data.int_val:(fl_int)args[2].data.float_val);
    int c=(int)(args[3].type==FL_TYPE_INT?args[3].data.int_val:(fl_int)args[3].data.float_val);
    int v=(int)(args[4].type==FL_TYPE_INT?args[4].data.int_val:(fl_int)args[4].data.float_val);
    int w=vv_get_int_field(args[0],"width"), ch=vv_get_int_field(args[0],"channels");
    uint8_t* d=vv_get_data(args[0]);
    if (!d||x<0||y<0||c<0||c>=ch) return fl_new_null();
    d[(y*w+x)*ch+c]=(uint8_t)(v<0?0:v>255?255:v);
    return fl_new_null();
}

fl_value_t fl_vision_info(fl_value_t* args, size_t argc) {
    if (argc<1) return fl_new_string("(no image)");
    int w=vv_get_int_field(args[0],"width"), h=vv_get_int_field(args[0],"height");
    int ch=vv_get_int_field(args[0],"channels");
    const char* cs=(ch==1)?"Gray":(ch==3)?"RGB":(ch==4)?"RGBA":"?";
    char buf[128];
    snprintf(buf,sizeof(buf),"Vector-Vision %dx%d %s (%d bytes)",w,h,cs,w*h*ch);
    return fl_new_string(buf);
}

/* vision_simd_caps() -> string: 컴파일 시점 CPU 기능 감지 */
fl_value_t fl_vision_simd_caps(fl_value_t* args, size_t argc) {
    (void)args; (void)argc;
    char caps[256]; strcpy(caps, "Vector-Vision SIMD: ");
#if defined(__AVX512F__)
    strcat(caps, "AVX-512 ");
#endif
#if defined(__AVX2__)
    strcat(caps, "AVX2(256bit) ");
#endif
#if defined(__SSE4_2__)
    strcat(caps, "SSE4.2 ");
#elif defined(__SSE2__)
    strcat(caps, "SSE2(128bit) ");
#endif
#if defined(__ARM_NEON)
    strcat(caps, "NEON(128bit) ");
#endif
#if !defined(__AVX2__) && !defined(__SSE2__) && !defined(__ARM_NEON)
    strcat(caps, "Scalar(fallback) ");
#endif
    strcat(caps, "| GCC -O3 -ftree-vectorize");
    return fl_new_string(caps);
}

/* ============================================================================
   PHASE 8: MOSS-Autodoc Standard Library
   Self-Documenting API Engine — Swagger/OpenAPI 대체 네이티브 구현
   외부 의존성: 0
   ============================================================================ */

#include "../include/autodoc.h"

/* autodoc_init(title, version, description) -> null */
fl_value_t fl_autodoc_init_builtin(fl_value_t* args, size_t argc) {
    const char *title   = (argc >= 1 && args[0].type == FL_TYPE_STRING) ? args[0].data.string_val : "FreeLang API";
    const char *version = (argc >= 2 && args[1].type == FL_TYPE_STRING) ? args[1].data.string_val : "1.0.0";
    const char *desc    = (argc >= 3 && args[2].type == FL_TYPE_STRING) ? args[2].data.string_val : "";
    fl_autodoc_init(title, version, desc);
    return fl_new_null();
}

/* autodoc_register(name, path, method, summary, tag?, returns?) -> int */
fl_value_t fl_autodoc_register_builtin(fl_value_t* args, size_t argc) {
    const char *name    = (argc >= 1 && args[0].type == FL_TYPE_STRING) ? args[0].data.string_val : "";
    const char *path    = (argc >= 2 && args[1].type == FL_TYPE_STRING) ? args[1].data.string_val : "/";
    const char *method  = (argc >= 3 && args[2].type == FL_TYPE_STRING) ? args[2].data.string_val : "GET";
    const char *summary = (argc >= 4 && args[3].type == FL_TYPE_STRING) ? args[3].data.string_val : "";
    const char *tag     = (argc >= 5 && args[4].type == FL_TYPE_STRING) ? args[4].data.string_val : "default";
    const char *returns = (argc >= 6 && args[5].type == FL_TYPE_STRING) ? args[5].data.string_val : "object";
    int idx = fl_autodoc_register(name, path, method, summary, tag, returns);
    return fl_new_int(idx);
}

/* autodoc_add_param(name, type, desc, required) -> null */
fl_value_t fl_autodoc_add_param_builtin(fl_value_t* args, size_t argc) {
    const char *name = (argc >= 1 && args[0].type == FL_TYPE_STRING) ? args[0].data.string_val : "";
    const char *type = (argc >= 2 && args[1].type == FL_TYPE_STRING) ? args[1].data.string_val : "string";
    const char *desc = (argc >= 3 && args[2].type == FL_TYPE_STRING) ? args[2].data.string_val : "";
    int required     = (argc >= 4 && args[3].type == FL_TYPE_BOOL)   ? args[3].data.bool_val  : 0;
    fl_autodoc_add_param(name, type, desc, required);
    return fl_new_null();
}

/* autodoc_json() -> string (OpenAPI 3.0 JSON) */
fl_value_t fl_autodoc_json_builtin(fl_value_t* args, size_t argc) {
    (void)args; (void)argc;
    char *json = fl_autodoc_to_openapi_json();
    if (!json) return fl_new_string("{}");
    fl_value_t result = fl_new_string(json);
    free(json);
    return result;
}

/* autodoc_html() -> string (내장 HTML UI) */
fl_value_t fl_autodoc_html_builtin(fl_value_t* args, size_t argc) {
    (void)args; (void)argc;
    char *html = fl_autodoc_render_html();
    if (!html) return fl_new_string("<html><body>AutoDoc Error</body></html>");
    fl_value_t result = fl_new_string(html);
    free(html);
    return result;
}

/* autodoc_routes_json() -> string */
fl_value_t fl_autodoc_routes_json_builtin(fl_value_t* args, size_t argc) {
    (void)args; (void)argc;
    char *json = fl_autodoc_routes_json();
    if (!json) return fl_new_string("[]");
    fl_value_t result = fl_new_string(json);
    free(json);
    return result;
}

/* autodoc_count() -> int */
fl_value_t fl_autodoc_count_builtin(fl_value_t* args, size_t argc) {
    (void)args; (void)argc;
    return fl_new_int(fl_autodoc_global.count);
}
