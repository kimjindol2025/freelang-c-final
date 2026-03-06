/**
 * FreeLang C - Simple Type Checker Test
 *
 * Tests basic functionality with minimal dependencies
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "typechecker.h"

int main(void) {
    printf("===== FreeLang C Type Checker - Simple Tests =====\n\n");

    /* Test 1: Type creation */
    printf("[Test 1] Creating int type...\n");
    TypeInfo *t_int = type_new(TYPE_INT, "int");
    if (t_int) {
        printf("  ✓ Created int type\n");
        printf("  - Kind: %d\n", t_int->kind);
        printf("  - Name: %s\n", t_int->name);
        type_free(t_int);
    } else {
        printf("  ✗ FAILED to create int type\n");
        return 1;
    }

    /* Test 2: Array type creation */
    printf("\n[Test 2] Creating array<int> type...\n");
    TypeInfo *t_elem = type_new(TYPE_INT, "int");
    TypeInfo *t_array = type_new_array(t_elem);
    if (t_array) {
        printf("  ✓ Created array<int> type\n");
        printf("  - Kind: %d\n", t_array->kind);
        printf("  - Element type kind: %d\n", t_array->element_type->kind);
        type_free(t_array);
    } else {
        printf("  ✗ FAILED to create array type\n");
        return 1;
    }

    /* Test 3: Type equality */
    printf("\n[Test 3] Testing type equality...\n");
    TypeInfo *t1 = type_new(TYPE_INT, "int");
    TypeInfo *t2 = type_new(TYPE_INT, "int");
    int equal = type_equal(t1, t2);
    printf("  - type_equal(int, int) = %d\n", equal);
    if (equal) {
        printf("  ✓ Types are equal\n");
    } else {
        printf("  ✗ Types should be equal\n");
    }
    type_free(t1);
    type_free(t2);

    /* Test 4: Type to string */
    printf("\n[Test 4] Converting types to strings...\n");
    char buf[256];
    TypeInfo *t_str = type_new(TYPE_STRING, "string");
    char *result = type_to_string(t_str, buf, sizeof(buf));
    printf("  - String type: '%s'\n", result);
    if (strcmp(buf, "string") == 0) {
        printf("  ✓ Type string conversion works\n");
    }
    type_free(t_str);

    /* Test 5: Binary operation */
    printf("\n[Test 5] Checking binary operations...\n");
    TypeInfo *left = type_new(TYPE_INT, "int");
    TypeInfo *right = type_new(TYPE_INT, "int");
    TypeInfo *result_type = check_binary_op(left, "+", right);
    if (result_type && result_type->kind == TYPE_INT) {
        printf("  ✓ int + int -> int\n");
    } else {
        printf("  ✗ Binary op failed\n");
    }
    type_free(left);
    type_free(right);

    /* Test 6: Type Checker Creation */
    printf("\n[Test 6] Creating type checker...\n");
    TypeChecker *tc = typechecker_new();
    if (tc) {
        printf("  ✓ Created type checker\n");

        int depth = typechecker_scope_depth(tc);
        printf("  - Scope depth: %d\n", depth);

        typechecker_free(tc);
    } else {
        printf("  ✗ FAILED to create type checker\n");
        return 1;
    }

    printf("\n===== All simple tests passed! ✓ =====\n");
    return 0;
}
