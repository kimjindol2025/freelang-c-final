/**
 * Test program for FreeLang C Standard Library (Phase 3-B)
 * Demonstrates stdlib function usage
 */

#include <stdio.h>
#include <stdlib.h>
#include "../include/freelang.h"
#include "../include/stdlib_fl.h"

int main() {
    printf("=== FreeLang C Standard Library Tests ===\n\n");

    /* Test 1: String Functions */
    printf("1. STRING FUNCTIONS\n");
    {
        fl_value_t str = fl_new_string("hello world");
        fl_value_t upper_str = fl_string_upper(&str, 1);
        printf("   Original: hello world\n");
        printf("   Uppercase: %s\n", upper_str.data.string_val);
        free(upper_str.data.string_val);

        fl_value_t substr = fl_string_slice(&str, 3);
        printf("   Slice [0:5]: %s\n", substr.data.string_val);
        free(substr.data.string_val);

        fl_value_t len_result = fl_string_length(&str, 1);
        printf("   Length: %ld\n", len_result.data.int_val);
        free(str.data.string_val);
    }

    /* Test 2: Array Functions */
    printf("\n2. ARRAY FUNCTIONS\n");
    {
        fl_array_t* arr = malloc(sizeof(fl_array_t));
        arr->elements = malloc(10 * sizeof(fl_value_t));
        arr->capacity = 10;
        arr->size = 0;

        /* Push elements */
        fl_value_t arr_val;
        arr_val.type = FL_TYPE_ARRAY;
        arr_val.data.array_val = arr;

        fl_value_t v1 = fl_new_int(10);
        fl_value_t v2 = fl_new_int(20);
        fl_value_t v3 = fl_new_int(30);

        fl_array_push(&arr_val, 2);
        arr->elements[arr->size - 1] = v1;
        arr->size = 1;

        fl_array_push(&arr_val, 2);
        arr->elements[arr->size - 1] = v2;
        arr->size = 2;

        fl_array_push(&arr_val, 2);
        arr->elements[arr->size - 1] = v3;
        arr->size = 3;

        fl_value_t arr_len = fl_array_length(&arr_val, 1);
        printf("   Array length: %ld\n", arr_len.data.int_val);

        fl_value_t joined = fl_array_join(&arr_val, 2);
        printf("   Array joined: %s\n", joined.data.string_val);
        free(joined.data.string_val);

        free(arr->elements);
        free(arr);
    }

    /* Test 3: Math Functions */
    printf("\n3. MATH FUNCTIONS\n");
    {
        fl_value_t num = fl_new_int(16);
        fl_value_t sqrt_result = fl_math_sqrt(&num, 1);
        printf("   sqrt(16) = %g\n", sqrt_result.data.float_val);

        fl_value_t base = fl_new_int(2);
        fl_value_t exp = fl_new_int(10);
        fl_value_t args[] = {base, exp};
        fl_value_t pow_result = fl_math_pow(args, 2);
        printf("   pow(2, 10) = %g\n", pow_result.data.float_val);

        fl_value_t pi_val = fl_new_float(3.14159);
        fl_value_t sin_result = fl_math_sin(&pi_val, 1);
        printf("   sin(3.14159) = %g\n", sin_result.data.float_val);
    }

    /* Test 4: Type Conversion */
    printf("\n4. TYPE CONVERSION\n");
    {
        fl_value_t num = fl_new_int(42);
        fl_value_t str_result = fl_string_convert(&num, 1);
        printf("   int(42) as string: %s\n", str_result.data.string_val);
        free(str_result.data.string_val);

        fl_value_t bool_result = fl_bool_convert(&num, 1);
        printf("   int(42) as bool: %s\n", bool_result.data.bool_val ? "true" : "false");

        fl_value_t null_val;
        null_val.type = FL_TYPE_NULL;
        fl_value_t type_result = fl_typeof(&null_val, 1);
        printf("   typeof(null): %s\n", type_result.data.string_val);
        free(type_result.data.string_val);
    }

    printf("\n=== All Tests Complete ===\n");
    return 0;
}
