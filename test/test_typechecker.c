/**
 * FreeLang C - Type Checker Tests
 *
 * Tests for:
 * - Type creation and management
 * - Symbol table and scope management
 * - Type checking and compatibility
 * - Type inference
 * - Error reporting
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "typechecker.h"

/* ============================================================================
   Test Utilities
   ============================================================================ */

static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        test_count++; \
        if (condition) { \
            test_passed++; \
            printf("  ✓ %s\n", message); \
        } else { \
            test_failed++; \
            printf("  ✗ FAILED: %s (line %d)\n", message, __LINE__); \
        } \
    } while (0)

#define TEST_SECTION(name) \
    printf("\n[Test %d] %s\n", ++test_group, name)

static int test_group = 0;

/* ============================================================================
   Test: Type Creation
   ============================================================================ */

static void test_type_creation(void) {
    TEST_SECTION("Type Creation");

    /* Test basic types */
    TypeInfo *t_int = type_new(TYPE_INT, "int");
    TEST_ASSERT(t_int != NULL, "Create int type");
    TEST_ASSERT(t_int->kind == TYPE_INT, "Int type has correct kind");
    TEST_ASSERT(strcmp(t_int->name, "int") == 0, "Int type has correct name");
    type_free(t_int);

    TypeInfo *t_float = type_new(TYPE_FLOAT, "float");
    TEST_ASSERT(t_float != NULL, "Create float type");
    TEST_ASSERT(t_float->kind == TYPE_FLOAT, "Float type has correct kind");
    type_free(t_float);

    /* Test array type */
    TypeInfo *t_elem = type_new(TYPE_INT, "int");
    TypeInfo *t_array = type_new_array(t_elem);
    TEST_ASSERT(t_array != NULL, "Create array<int> type");
    TEST_ASSERT(t_array->kind == TYPE_ARRAY, "Array type has correct kind");
    TEST_ASSERT(t_array->element_type != NULL, "Array has element type");
    TEST_ASSERT(t_array->element_type->kind == TYPE_INT, "Element type is int");
    type_free(t_array);

    /* Test function type */
    TypeInfo *t_ret = type_new(TYPE_INT, "int");
    TypeInfo *params[] = {
        type_new(TYPE_INT, "int"),
        type_new(TYPE_INT, "int")
    };
    TypeInfo *t_func = type_new_function(t_ret, params, 2);
    TEST_ASSERT(t_func != NULL, "Create function type");
    TEST_ASSERT(t_func->kind == TYPE_FUNCTION, "Function has correct kind");
    TEST_ASSERT(t_func->param_count == 2, "Function has 2 parameters");
    TEST_ASSERT(t_func->return_type != NULL, "Function has return type");
    type_free(t_func);

    /* Test generic type */
    TypeInfo *t_arg = type_new(TYPE_INT, "int");
    TypeInfo *t_generic = type_new_generic("T", t_arg);
    TEST_ASSERT(t_generic != NULL, "Create generic<T> type");
    TEST_ASSERT(t_generic->kind == TYPE_GENERIC, "Generic has correct kind");
    TEST_ASSERT(t_generic->generic_arg != NULL, "Generic has argument");
    type_free(t_generic);
}

/* ============================================================================
   Test: Type Equality
   ============================================================================ */

static void test_type_equality(void) {
    TEST_SECTION("Type Equality & Comparison");

    TypeInfo *t1 = type_new(TYPE_INT, "int");
    TypeInfo *t2 = type_new(TYPE_INT, "int");
    TypeInfo *t3 = type_new(TYPE_FLOAT, "float");

    TEST_ASSERT(type_equal(t1, t2), "Same int types are equal");
    TEST_ASSERT(!type_equal(t1, t3), "Different types are not equal");
    TEST_ASSERT(type_equal(t1, t1), "Type is equal to itself");

    type_free(t1);
    type_free(t2);
    type_free(t3);

    /* Array equality */
    TypeInfo *arr1 = type_new_array(type_new(TYPE_INT, "int"));
    TypeInfo *arr2 = type_new_array(type_new(TYPE_INT, "int"));
    TypeInfo *arr3 = type_new_array(type_new(TYPE_STRING, "string"));

    TEST_ASSERT(type_equal(arr1, arr2), "array<int> types are equal");
    TEST_ASSERT(!type_equal(arr1, arr3), "array<int> != array<string>");

    type_free(arr1);
    type_free(arr2);
    type_free(arr3);

    /* Function equality */
    TypeInfo *f1 = type_new_function(
        type_new(TYPE_INT, "int"),
        (TypeInfo *[]){type_new(TYPE_INT, "int")}, 1
    );
    TypeInfo *f2 = type_new_function(
        type_new(TYPE_INT, "int"),
        (TypeInfo *[]){type_new(TYPE_INT, "int")}, 1
    );

    TEST_ASSERT(type_equal(f1, f2), "Same function types are equal");

    type_free(f1);
    type_free(f2);
}

/* ============================================================================
   Test: Type Compatibility
   ============================================================================ */

static void test_type_compatibility(void) {
    TEST_SECTION("Type Compatibility");

    TypeInfo *t_int = type_new(TYPE_INT, "int");
    TypeInfo *t_float = type_new(TYPE_FLOAT, "float");

    TEST_ASSERT(type_compatible(t_int, t_int), "int compatible with int");
    TEST_ASSERT(type_compatible(t_int, t_float), "int compatible with float");
    TEST_ASSERT(!type_compatible(t_float, t_int), "float not compatible with int");

    type_free(t_int);
    type_free(t_float);

    /* Null compatibility */
    TypeInfo *t_null = type_new(TYPE_NULL, "null");
    TypeInfo *t_nullable = type_new(TYPE_INT, "int");
    t_nullable->is_nullable = 1;

    TEST_ASSERT(type_compatible(t_null, t_nullable), "null compatible with nullable");
    TEST_ASSERT(!type_compatible(t_null, type_new(TYPE_INT, "int")),
                "null not compatible with non-nullable int");

    type_free(t_null);
    type_free(t_nullable);
}

/* ============================================================================
   Test: Type to String
   ============================================================================ */

static void test_type_to_string(void) {
    TEST_SECTION("Type to String Conversion");

    char buf[256];

    /* Basic types */
    TypeInfo *t_int = type_new(TYPE_INT, "int");
    type_to_string(t_int, buf, sizeof(buf));
    TEST_ASSERT(strcmp(buf, "int") == 0, "int -> 'int'");
    type_free(t_int);

    TypeInfo *t_string = type_new(TYPE_STRING, "string");
    type_to_string(t_string, buf, sizeof(buf));
    TEST_ASSERT(strcmp(buf, "string") == 0, "string -> 'string'");
    type_free(t_string);

    /* Array type */
    TypeInfo *t_array = type_new_array(type_new(TYPE_INT, "int"));
    type_to_string(t_array, buf, sizeof(buf));
    TEST_ASSERT(strcmp(buf, "array<int>") == 0, "array<int> -> 'array<int>'");
    type_free(t_array);

    /* Function type */
    TypeInfo *t_func = type_new_function(
        type_new(TYPE_BOOL, "bool"),
        (TypeInfo *[]){type_new(TYPE_INT, "int"), type_new(TYPE_INT, "int")}, 2
    );
    type_to_string(t_func, buf, sizeof(buf));
    TEST_ASSERT(strstr(buf, "->") != NULL, "Function type contains '->'");
    type_free(t_func);
}

/* ============================================================================
   Test: Binary Operations
   ============================================================================ */

static void test_binary_operations(void) {
    TEST_SECTION("Binary Operation Type Checking");

    TypeInfo *t_int = type_new(TYPE_INT, "int");
    TypeInfo *t_float = type_new(TYPE_FLOAT, "float");
    TypeInfo *t_string = type_new(TYPE_STRING, "string");
    TypeInfo *t_bool = type_new(TYPE_BOOL, "bool");

    /* Arithmetic */
    TypeInfo *result = check_binary_op(t_int, "+", t_int);
    TEST_ASSERT(result != NULL && result->kind == TYPE_INT, "int + int -> int");

    result = check_binary_op(t_int, "+", t_float);
    TEST_ASSERT(result != NULL && result->kind == TYPE_FLOAT, "int + float -> float");

    result = check_binary_op(t_string, "+", t_string);
    TEST_ASSERT(result != NULL && result->kind == TYPE_STRING, "string + string -> string");

    /* Comparison */
    result = check_binary_op(t_int, "==", t_int);
    TEST_ASSERT(result != NULL && result->kind == TYPE_BOOL, "int == int -> bool");

    result = check_binary_op(t_int, "<", t_float);
    TEST_ASSERT(result != NULL && result->kind == TYPE_BOOL, "int < float -> bool");

    /* Invalid operations */
    result = check_binary_op(t_bool, "+", t_bool);
    TEST_ASSERT(result == NULL, "bool + bool is invalid");

    type_free(t_int);
    type_free(t_float);
    type_free(t_string);
    type_free(t_bool);
}

/* ============================================================================
   Test: Unary Operations
   ============================================================================ */

static void test_unary_operations(void) {
    TEST_SECTION("Unary Operation Type Checking");

    TypeInfo *t_int = type_new(TYPE_INT, "int");
    TypeInfo *t_float = type_new(TYPE_FLOAT, "float");
    TypeInfo *t_bool = type_new(TYPE_BOOL, "bool");

    /* Negation */
    TypeInfo *result = check_unary_op("-", t_int);
    TEST_ASSERT(result != NULL && result->kind == TYPE_INT, "- int -> int");

    result = check_unary_op("-", t_float);
    TEST_ASSERT(result != NULL && result->kind == TYPE_FLOAT, "- float -> float");

    /* Logical NOT */
    result = check_unary_op("!", t_bool);
    TEST_ASSERT(result != NULL && result->kind == TYPE_BOOL, "! bool -> bool");

    /* Invalid operations */
    result = check_unary_op("-", t_bool);
    TEST_ASSERT(result == NULL, "- bool is invalid");

    type_free(t_int);
    type_free(t_float);
    type_free(t_bool);
}

/* ============================================================================
   Test: Symbol Table & Scopes
   ============================================================================ */

static void test_symbol_table(void) {
    TEST_SECTION("Symbol Table Management");

    TypeChecker *tc = typechecker_new();
    TEST_ASSERT(tc != NULL, "Create type checker");
    TEST_ASSERT(typechecker_scope_depth(tc) == 0, "Start in global scope (depth 0)");

    /* Define symbols in global scope */
    TypeInfo *t_int = type_new(TYPE_INT, "int");
    int result = typechecker_define(tc, "x", t_int);
    TEST_ASSERT(result == 0, "Define variable 'x' in global scope");

    /* Lookup symbol */
    Symbol *sym = typechecker_lookup(tc, "x");
    TEST_ASSERT(sym != NULL, "Lookup finds 'x'");
    TEST_ASSERT(sym->type->kind == TYPE_INT, "Symbol has correct type");

    /* Duplicate definition (should fail) */
    result = typechecker_define(tc, "x", type_new(TYPE_INT, "int"));
    TEST_ASSERT(result != 0, "Duplicate definition fails");

    /* Push new scope */
    typechecker_push_scope(tc);
    TEST_ASSERT(typechecker_scope_depth(tc) == 1, "Scope depth increases to 1");

    /* Define in nested scope */
    result = typechecker_define(tc, "y", type_new(TYPE_FLOAT, "float"));
    TEST_ASSERT(result == 0, "Define 'y' in nested scope");

    /* Lookup across scopes */
    sym = typechecker_lookup(tc, "x");
    TEST_ASSERT(sym != NULL, "Can lookup parent scope symbol 'x'");

    sym = typechecker_lookup(tc, "y");
    TEST_ASSERT(sym != NULL && sym->type->kind == TYPE_FLOAT, "Can lookup local 'y'");

    /* Shadowing */
    result = typechecker_define(tc, "x", type_new(TYPE_STRING, "string"));
    TEST_ASSERT(result == 0, "Shadowing allowed in nested scope");

    sym = typechecker_lookup(tc, "x");
    TEST_ASSERT(sym != NULL && sym->type->kind == TYPE_STRING,
                "Lookup finds shadowing symbol");

    /* Pop scope */
    result = typechecker_pop_scope(tc);
    TEST_ASSERT(result == 0, "Pop scope succeeds");
    TEST_ASSERT(typechecker_scope_depth(tc) == 0, "Back to global scope");

    /* Lookup after pop */
    sym = typechecker_lookup(tc, "x");
    TEST_ASSERT(sym != NULL && sym->type->kind == TYPE_INT,
                "After pop, 'x' has original type");

    sym = typechecker_lookup(tc, "y");
    TEST_ASSERT(sym == NULL, "'y' no longer accessible");

    typechecker_free(tc);
}

/* ============================================================================
   Test: Function Definition
   ============================================================================ */

static void test_function_definition(void) {
    TEST_SECTION("Function Definition & Calls");

    TypeChecker *tc = typechecker_new();

    /* Define function: (int, int) -> int */
    int result = typechecker_define_function(tc, "add",
        type_new(TYPE_INT, "int"),
        (TypeInfo *[]){type_new(TYPE_INT, "int"), type_new(TYPE_INT, "int")}, 2
    );
    TEST_ASSERT(result == 0, "Define function 'add'");

    Symbol *sym = typechecker_lookup(tc, "add");
    TEST_ASSERT(sym != NULL, "Lookup finds 'add'");
    TEST_ASSERT(sym->is_function, "'add' is marked as function");
    TEST_ASSERT(sym->type->kind == TYPE_FUNCTION, "'add' has function type");
    TEST_ASSERT(sym->type->param_count == 2, "'add' has 2 parameters");

    /* Test function call checking */
    TypeInfo *t_int1 = type_new(TYPE_INT, "int");
    TypeInfo *t_int2 = type_new(TYPE_INT, "int");
    TypeInfo *ret = check_function_call(sym->type, (TypeInfo *[]){t_int1, t_int2}, 2);
    TEST_ASSERT(ret != NULL && ret->kind == TYPE_INT, "Valid function call returns int");

    /* Wrong argument count */
    ret = check_function_call(sym->type, (TypeInfo *[]){t_int1}, 1);
    TEST_ASSERT(ret == NULL, "Wrong argument count fails");

    type_free(t_int1);
    type_free(t_int2);
    typechecker_free(tc);
}

/* ============================================================================
   Test: Type Definitions
   ============================================================================ */

static void test_type_definitions(void) {
    TEST_SECTION("User-Defined Types");

    TypeChecker *tc = typechecker_new();

    /* Define custom type */
    int result = typechecker_define_type(tc, "Point");
    TEST_ASSERT(result == 0, "Define custom type 'Point'");

    Symbol *sym = typechecker_lookup(tc, "Point");
    TEST_ASSERT(sym != NULL, "Lookup finds 'Point'");
    TEST_ASSERT(sym->is_type, "'Point' is marked as type");

    /* Duplicate type definition */
    result = typechecker_define_type(tc, "Point");
    TEST_ASSERT(result != 0, "Duplicate type definition fails");

    typechecker_free(tc);
}

/* ============================================================================
   Test: Error Reporting
   ============================================================================ */

static void test_error_reporting(void) {
    TEST_SECTION("Error Reporting");

    TypeChecker *tc = typechecker_new();

    TEST_ASSERT(typechecker_error_count(tc) == 0, "Start with no errors");

    /* Report errors */
    typechecker_error(tc, 10, 5, "Undefined variable 'x'");
    TEST_ASSERT(typechecker_error_count(tc) == 1, "Error count increments");

    typechecker_error(tc, 15, 10, "Type mismatch: int expected, string found");
    TEST_ASSERT(typechecker_error_count(tc) == 2, "Multiple errors collected");

    /* Get error */
    TypeCheckError *err = typechecker_error_get(tc, 0);
    TEST_ASSERT(err != NULL, "Retrieve first error");
    TEST_ASSERT(err->line == 10, "Error has correct line");
    TEST_ASSERT(err->col == 5, "Error has correct column");
    TEST_ASSERT(strcmp(err->message, "Undefined variable 'x'") == 0,
                "Error has correct message");

    err = typechecker_error_get(tc, 1);
    TEST_ASSERT(err != NULL && err->line == 15, "Retrieve second error");

    err = typechecker_error_get(tc, 999);
    TEST_ASSERT(err == NULL, "Invalid index returns NULL");

    typechecker_free(tc);
}

/* ============================================================================
   Test: Array Access Type Checking
   ============================================================================ */

static void test_array_access(void) {
    TEST_SECTION("Array Access Type Checking");

    TypeInfo *t_int = type_new(TYPE_INT, "int");
    TypeInfo *t_array_int = type_new_array(type_new(TYPE_INT, "int"));
    TypeInfo *t_string = type_new(TYPE_STRING, "string");

    /* Valid array access */
    TypeInfo *result = check_array_access(t_array_int, t_int);
    TEST_ASSERT(result != NULL && result->kind == TYPE_INT,
                "array<int>[int] -> int");

    /* Invalid: non-integer index */
    result = check_array_access(t_array_int, t_string);
    TEST_ASSERT(result == NULL, "array[string] is invalid");

    /* Invalid: non-array type */
    result = check_array_access(t_int, t_int);
    TEST_ASSERT(result == NULL, "non-array access is invalid");

    type_free(t_int);
    type_free(t_array_int);
    type_free(t_string);
}

/* ============================================================================
   Test: Generic Type Operations
   ============================================================================ */

static void test_generic_types(void) {
    TEST_SECTION("Generic Type Operations");

    /* Create generic identity function: <T> (T) -> T */
    TypeInfo *t_generic = type_new_generic("T", NULL);
    TypeInfo *t_func = type_new_function(
        t_generic,
        (TypeInfo *[]){type_new_generic("T", NULL)}, 1
    );

    TEST_ASSERT(t_func != NULL, "Create generic function");
    TEST_ASSERT(t_func->kind == TYPE_FUNCTION, "Generic function is function type");

    char buf[256];
    type_to_string(t_func, buf, sizeof(buf));
    TEST_ASSERT(strlen(buf) > 0, "Generic function converts to string");

    type_free(t_func);

    /* Generic array */
    TypeInfo *t_arr_generic = type_new_array(type_new_generic("T", NULL));
    TEST_ASSERT(t_arr_generic != NULL, "Create generic array type");

    type_to_string(t_arr_generic, buf, sizeof(buf));
    TEST_ASSERT(strstr(buf, "array") != NULL, "Generic array string representation");

    type_free(t_arr_generic);
}

/* ============================================================================
   Main Test Runner
   ============================================================================ */

int main(void) {
    printf("===== FreeLang C Type Checker Tests =====\n");

    printf("\n✓ Basic type creation tests...\n");
    test_type_creation();

    printf("\n✓ Type equality tests...\n");
    test_type_equality();

    printf("\n✓ Type compatibility tests...\n");
    test_type_compatibility();

    printf("\n✓ Type to string tests...\n");
    test_type_to_string();

    printf("\n✓ Binary operation tests...\n");
    test_binary_operations();

    printf("\n✓ Unary operation tests...\n");
    test_unary_operations();

    printf("\n✓ Symbol table tests...\n");
    test_symbol_table();

    printf("\n✓ Function definition tests...\n");
    test_function_definition();

    printf("\n✓ Type definition tests...\n");
    test_type_definitions();

    printf("\n✓ Error reporting tests...\n");
    test_error_reporting();

    printf("\n✓ Array access tests...\n");
    test_array_access();

    printf("\n✓ Generic type tests...\n");
    test_generic_types();

    printf("\n===== Test Summary =====\n");
    printf("Total: %d, Passed: %d, Failed: %d\n",
           test_count, test_passed, test_failed);

    if (test_failed == 0) {
        printf("\nAll tests passed! ✓\n");
        return 0;
    } else {
        printf("\n%d test(s) failed ✗\n", test_failed);
        return 1;
    }
}
