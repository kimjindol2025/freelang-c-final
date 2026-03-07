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
#include "../include/stdlib_fl.h"
#include "../include/runtime.h"

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
