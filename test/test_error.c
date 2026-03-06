/**
 * FreeLang Error Reporter Tests
 * Phase 2-C: Advanced Error Reporter (Rust compiler style)
 *
 * Tests 15+ error types with proper formatting and annotations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/error.h"

/* Test counters */
static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

/* Test helper macros */
#define TEST_START(name) \
    do { \
        test_count++; \
        printf("\n[Test %d] %s\n", test_count, name); \
        printf("─────────────────────────────────────────\n"); \
    } while(0)

#define TEST_PASS() \
    do { \
        pass_count++; \
        printf(ANSI_GREEN "✓ PASS\n" ANSI_RESET); \
    } while(0)

#define TEST_FAIL(msg) \
    do { \
        fail_count++; \
        printf(ANSI_RED "✗ FAIL: %s\n" ANSI_RESET, msg); \
    } while(0)

#define ANSI_GREEN   "\x1b[32m"
#define ANSI_RED     "\x1b[31m"
#define ANSI_RESET   "\x1b[0m"

/* =================================================================== */
/* Test 1: Basic Error Reporter Creation                              */
/* =================================================================== */
void test_error_reporter_create() {
    TEST_START("Error reporter creation");

    fl_error_reporter_t* er = fl_error_reporter_new();
    if (!er) {
        TEST_FAIL("Failed to create error reporter");
        return;
    }

    if (fl_error_reporter_error_count(er) != 0) {
        TEST_FAIL("Error count should be 0 initially");
        fl_error_reporter_free(er);
        return;
    }

    fl_error_reporter_free(er);
    TEST_PASS();
}

/* =================================================================== */
/* Test 2: E001 - Undefined Variable                                  */
/* =================================================================== */
void test_e001_undefined_variable() {
    TEST_START("E001 - Undefined variable");

    const char* source =
        "let y = 10;\n"
        "let z = x + y;\n";

    fl_error_reporter_t* er = fl_error_reporter_new();
    fl_error_reporter_set_source(er, "main.fl", source);

    fl_diagnostic_t* d = fl_error_reporter_add(er, FL_DIAG_E001,
                                                "undefined variable 'x'",
                                                2, 9);
    if (!d) {
        TEST_FAIL("Failed to add diagnostic");
        fl_error_reporter_free(er);
        return;
    }

    fl_error_diagnostic_annotate(d, 9, 10, "not found in this scope");
    fl_error_diagnostic_hint(d, "did you mean 'y'?");

    printf("\nExpected output:\n");
    fl_error_diagnostic_print(d, &er->source);

    if (fl_error_reporter_error_count(er) != 1) {
        TEST_FAIL("Error count should be 1");
        fl_error_reporter_free(er);
        return;
    }

    fl_error_reporter_free(er);
    TEST_PASS();
}

/* =================================================================== */
/* Test 3: E002 - Undefined Function                                  */
/* =================================================================== */
void test_e002_undefined_function() {
    TEST_START("E002 - Undefined function");

    const char* source =
        "foo();  // undefined function\n";

    fl_error_reporter_t* er = fl_error_reporter_new();
    fl_error_reporter_set_source(er, "main.fl", source);

    fl_diagnostic_t* d = fl_error_reporter_add(er, FL_DIAG_E002,
                                                "undefined function 'foo'",
                                                1, 0);
    if (!d) {
        TEST_FAIL("Failed to add diagnostic");
        fl_error_reporter_free(er);
        return;
    }

    fl_error_diagnostic_annotate(d, 0, 3, "function not found");

    printf("\nExpected output:\n");
    fl_error_diagnostic_print(d, &er->source);

    fl_error_reporter_free(er);
    TEST_PASS();
}

/* =================================================================== */
/* Test 4: E003 - Type Mismatch                                       */
/* =================================================================== */
void test_e003_type_mismatch() {
    TEST_START("E003 - Type mismatch");

    const char* source =
        "let x: int = \"hello\";\n";

    fl_error_reporter_t* er = fl_error_reporter_new();
    fl_error_reporter_set_source(er, "main.fl", source);

    fl_diagnostic_t* d = fl_error_reporter_add(er, FL_DIAG_E003,
                                                "type mismatch",
                                                1, 13);
    if (!d) {
        TEST_FAIL("Failed to add diagnostic");
        fl_error_reporter_free(er);
        return;
    }

    fl_error_diagnostic_annotate(d, 13, 20, "expected `int`, found `string`");
    fl_error_diagnostic_hint(d, "you can convert with `int()`");

    printf("\nExpected output:\n");
    fl_error_diagnostic_print(d, &er->source);

    fl_error_reporter_free(er);
    TEST_PASS();
}

/* =================================================================== */
/* Test 5: E005 - Invalid Argument Count                              */
/* =================================================================== */
void test_e005_invalid_argument_count() {
    TEST_START("E005 - Invalid argument count");

    const char* source =
        "fn add(a: int, b: int): int { a + b }\n"
        "add(1, 2, 3);\n";

    fl_error_reporter_t* er = fl_error_reporter_new();
    fl_error_reporter_set_source(er, "main.fl", source);

    fl_diagnostic_t* d = fl_error_reporter_add(er, FL_DIAG_E005,
                                                "invalid argument count",
                                                2, 0);
    if (!d) {
        TEST_FAIL("Failed to add diagnostic");
        fl_error_reporter_free(er);
        return;
    }

    fl_error_diagnostic_annotate(d, 0, 3, "function takes 2 arguments but 3 were given");
    fl_error_diagnostic_hint(d, "function defined at main.fl:1");

    printf("\nExpected output:\n");
    fl_error_diagnostic_print(d, &er->source);

    fl_error_reporter_free(er);
    TEST_PASS();
}

/* =================================================================== */
/* Test 6: E009 - Syntax Error                                        */
/* =================================================================== */
void test_e009_syntax_error() {
    TEST_START("E009 - Syntax error");

    const char* source =
        "fn foo(x: int) {\n";

    fl_error_reporter_t* er = fl_error_reporter_new();
    fl_error_reporter_set_source(er, "main.fl", source);

    fl_diagnostic_t* d = fl_error_reporter_add(er, FL_DIAG_E009,
                                                "syntax error",
                                                1, 17);
    if (!d) {
        TEST_FAIL("Failed to add diagnostic");
        fl_error_reporter_free(er);
        return;
    }

    fl_error_diagnostic_annotate(d, 16, 17, "expected '}'");
    fl_error_diagnostic_hint(d, "add closing brace");

    printf("\nExpected output:\n");
    fl_error_diagnostic_print(d, &er->source);

    fl_error_reporter_free(er);
    TEST_PASS();
}

/* =================================================================== */
/* Test 7: E006 - Duplicate Definition                                */
/* =================================================================== */
void test_e006_duplicate_definition() {
    TEST_START("E006 - Duplicate definition");

    const char* source =
        "let x = 1;\n"
        "let x = 2;\n";

    fl_error_reporter_t* er = fl_error_reporter_new();
    fl_error_reporter_set_source(er, "main.fl", source);

    fl_diagnostic_t* d = fl_error_reporter_add(er, FL_DIAG_E006,
                                                "duplicate definition of 'x'",
                                                2, 4);
    if (!d) {
        TEST_FAIL("Failed to add diagnostic");
        fl_error_reporter_free(er);
        return;
    }

    fl_error_diagnostic_annotate(d, 4, 5, "previous definition");
    fl_error_diagnostic_hint(d, "first definition at main.fl:1:4");

    printf("\nExpected output:\n");
    fl_error_diagnostic_print(d, &er->source);

    fl_error_reporter_free(er);
    TEST_PASS();
}

/* =================================================================== */
/* Test 8: E013 - Divide by Zero                                      */
/* =================================================================== */
void test_e013_divide_by_zero() {
    TEST_START("E013 - Divide by zero");

    const char* source =
        "let result = 10 / 0;\n";

    fl_error_reporter_t* er = fl_error_reporter_new();
    fl_error_reporter_set_source(er, "main.fl", source);

    fl_diagnostic_t* d = fl_error_reporter_add(er, FL_DIAG_E013,
                                                "divide by zero",
                                                1, 16);
    if (!d) {
        TEST_FAIL("Failed to add diagnostic");
        fl_error_reporter_free(er);
        return;
    }

    fl_error_diagnostic_annotate(d, 16, 17, "divisor must be non-zero");

    printf("\nExpected output:\n");
    fl_error_diagnostic_print(d, &er->source);

    fl_error_reporter_free(er);
    TEST_PASS();
}

/* =================================================================== */
/* Test 9: E014 - Unreachable Code                                    */
/* =================================================================== */
void test_e014_unreachable_code() {
    TEST_START("E014 - Unreachable code");

    const char* source =
        "fn foo() {\n"
        "    return 42;\n"
        "    let x = 1;\n"
        "}\n";

    fl_error_reporter_t* er = fl_error_reporter_new();
    fl_error_reporter_set_source(er, "main.fl", source);

    fl_diagnostic_t* d = fl_error_reporter_add(er, FL_DIAG_E014,
                                                "unreachable code",
                                                3, 4);
    if (!d) {
        TEST_FAIL("Failed to add diagnostic");
        fl_error_reporter_free(er);
        return;
    }

    fl_error_diagnostic_annotate(d, 4, 11, "unreachable statement");
    fl_error_diagnostic_hint(d, "code after `return` is never executed");

    printf("\nExpected output:\n");
    fl_error_diagnostic_print(d, &er->source);

    fl_error_reporter_free(er);
    TEST_PASS();
}

/* =================================================================== */
/* Test 10: E015 - Missing Return                                     */
/* =================================================================== */
void test_e015_missing_return() {
    TEST_START("E015 - Missing return");

    const char* source =
        "fn foo(): int {\n"
        "    let x = 42;\n"
        "}\n";

    fl_error_reporter_t* er = fl_error_reporter_new();
    fl_error_reporter_set_source(er, "main.fl", source);

    fl_diagnostic_t* d = fl_error_reporter_add(er, FL_DIAG_E015,
                                                "missing return statement",
                                                3, 0);
    if (!d) {
        TEST_FAIL("Failed to add diagnostic");
        fl_error_reporter_free(er);
        return;
    }

    fl_error_diagnostic_annotate(d, 0, 1, "function expects `int` return");
    fl_error_diagnostic_hint(d, "add `return` statement before closing brace");

    printf("\nExpected output:\n");
    fl_error_diagnostic_print(d, &er->source);

    fl_error_reporter_free(er);
    TEST_PASS();
}

/* =================================================================== */
/* Test 11: E010 - Unexpected Token                                   */
/* =================================================================== */
void test_e010_unexpected_token() {
    TEST_START("E010 - Unexpected token");

    const char* source =
        "fn 123() {}\n";

    fl_error_reporter_t* er = fl_error_reporter_new();
    fl_error_reporter_set_source(er, "main.fl", source);

    fl_diagnostic_t* d = fl_error_reporter_add(er, FL_DIAG_E010,
                                                "unexpected token",
                                                1, 3);
    if (!d) {
        TEST_FAIL("Failed to add diagnostic");
        fl_error_reporter_free(er);
        return;
    }

    fl_error_diagnostic_annotate(d, 3, 6, "expected identifier");

    printf("\nExpected output:\n");
    fl_error_diagnostic_print(d, &er->source);

    fl_error_reporter_free(er);
    TEST_PASS();
}

/* =================================================================== */
/* Test 12: E011 - Unclosed Bracket                                   */
/* =================================================================== */
void test_e011_unclosed_bracket() {
    TEST_START("E011 - Unclosed bracket");

    const char* source =
        "let arr = [1, 2, 3\n";

    fl_error_reporter_t* er = fl_error_reporter_new();
    fl_error_reporter_set_source(er, "main.fl", source);

    fl_diagnostic_t* d = fl_error_reporter_add(er, FL_DIAG_E011,
                                                "unclosed bracket",
                                                1, 19);
    if (!d) {
        TEST_FAIL("Failed to add diagnostic");
        fl_error_reporter_free(er);
        return;
    }

    fl_error_diagnostic_annotate(d, 19, 19, "missing `]`");
    fl_error_diagnostic_hint(d, "opened at main.fl:1:9");

    printf("\nExpected output:\n");
    fl_error_diagnostic_print(d, &er->source);

    fl_error_reporter_free(er);
    TEST_PASS();
}

/* =================================================================== */
/* Test 13: E004 - Invalid Operator                                   */
/* =================================================================== */
void test_e004_invalid_operator() {
    TEST_START("E004 - Invalid operator");

    const char* source =
        "let s = \"hello\" - \"world\";\n";

    fl_error_reporter_t* er = fl_error_reporter_new();
    fl_error_reporter_set_source(er, "main.fl", source);

    fl_diagnostic_t* d = fl_error_reporter_add(er, FL_DIAG_E004,
                                                "invalid operator",
                                                1, 19);
    if (!d) {
        TEST_FAIL("Failed to add diagnostic");
        fl_error_reporter_free(er);
        return;
    }

    fl_error_diagnostic_annotate(d, 18, 19, "cannot subtract strings");

    printf("\nExpected output:\n");
    fl_error_diagnostic_print(d, &er->source);

    fl_error_reporter_free(er);
    TEST_PASS();
}

/* =================================================================== */
/* Test 14: E007 - Invalid Index                                      */
/* =================================================================== */
void test_e007_invalid_index() {
    TEST_START("E007 - Invalid index");

    const char* source =
        "let arr = [1, 2, 3];\n"
        "let x = arr[\"key\"];\n";

    fl_error_reporter_t* er = fl_error_reporter_new();
    fl_error_reporter_set_source(er, "main.fl", source);

    fl_diagnostic_t* d = fl_error_reporter_add(er, FL_DIAG_E007,
                                                "invalid index",
                                                2, 14);
    if (!d) {
        TEST_FAIL("Failed to add diagnostic");
        fl_error_reporter_free(er);
        return;
    }

    fl_error_diagnostic_annotate(d, 14, 19, "array index must be integer");

    printf("\nExpected output:\n");
    fl_error_diagnostic_print(d, &er->source);

    fl_error_reporter_free(er);
    TEST_PASS();
}

/* =================================================================== */
/* Test 15: E008 - Invalid Member Access                              */
/* =================================================================== */
void test_e008_invalid_member_access() {
    TEST_START("E008 - Invalid member access");

    const char* source =
        "let x = 5;\n"
        "let y = x.foo;\n";

    fl_error_reporter_t* er = fl_error_reporter_new();
    fl_error_reporter_set_source(er, "main.fl", source);

    fl_diagnostic_t* d = fl_error_reporter_add(er, FL_DIAG_E008,
                                                "invalid member access",
                                                2, 8);
    if (!d) {
        TEST_FAIL("Failed to add diagnostic");
        fl_error_reporter_free(er);
        return;
    }

    fl_error_diagnostic_annotate(d, 8, 11, "number type has no members");

    printf("\nExpected output:\n");
    fl_error_diagnostic_print(d, &er->source);

    fl_error_reporter_free(er);
    TEST_PASS();
}

/* =================================================================== */
/* Test 16: E012 - Invalid Escape Sequence                            */
/* =================================================================== */
void test_e012_invalid_escape_sequence() {
    TEST_START("E012 - Invalid escape sequence");

    const char* source =
        "let s = \"hello\\x world\";\n";

    fl_error_reporter_t* er = fl_error_reporter_new();
    fl_error_reporter_set_source(er, "main.fl", source);

    fl_diagnostic_t* d = fl_error_reporter_add(er, FL_DIAG_E012,
                                                "invalid escape sequence",
                                                1, 17);
    if (!d) {
        TEST_FAIL("Failed to add diagnostic");
        fl_error_reporter_free(er);
        return;
    }

    fl_error_diagnostic_annotate(d, 16, 18, "unknown escape sequence `\\x`");

    printf("\nExpected output:\n");
    fl_error_diagnostic_print(d, &er->source);

    fl_error_reporter_free(er);
    TEST_PASS();
}

/* =================================================================== */
/* Test 17: Multiple Diagnostics                                      */
/* =================================================================== */
void test_multiple_diagnostics() {
    TEST_START("Multiple diagnostics in one reporter");

    const char* source =
        "let y = 10;\n"
        "let z = x + y;\n"
        "foo();\n";

    fl_error_reporter_t* er = fl_error_reporter_new();
    fl_error_reporter_set_source(er, "main.fl", source);

    /* First error */
    fl_diagnostic_t* d1 = fl_error_reporter_add(er, FL_DIAG_E001,
                                                 "undefined variable 'x'",
                                                 2, 9);
    if (!d1) {
        TEST_FAIL("Failed to add first diagnostic");
        fl_error_reporter_free(er);
        return;
    }
    fl_error_diagnostic_annotate(d1, 9, 10, "not found in this scope");

    /* Second error */
    fl_diagnostic_t* d2 = fl_error_reporter_add(er, FL_DIAG_E002,
                                                 "undefined function 'foo'",
                                                 3, 0);
    if (!d2) {
        TEST_FAIL("Failed to add second diagnostic");
        fl_error_reporter_free(er);
        return;
    }
    fl_error_diagnostic_annotate(d2, 0, 3, "function not found");

    printf("\nExpected output (2 errors):\n");
    fl_error_reporter_print_all(er);

    if (fl_error_reporter_error_count(er) != 2) {
        TEST_FAIL("Should have 2 errors");
        fl_error_reporter_free(er);
        return;
    }

    fl_error_reporter_free(er);
    TEST_PASS();
}

/* =================================================================== */
/* Test 18: Source Line Extraction                                    */
/* =================================================================== */
void test_source_line_extraction() {
    TEST_START("Source line extraction");

    const char* source =
        "line 1\n"
        "line 2\n"
        "line 3\n";

    fl_error_reporter_t* er = fl_error_reporter_new();
    fl_error_reporter_set_source(er, "test.fl", source);

    const char* line1 = fl_error_reporter_get_line(er, 1);
    const char* line2 = fl_error_reporter_get_line(er, 2);
    const char* line3 = fl_error_reporter_get_line(er, 3);

    if (!line1 || strcmp(line1, "line 1") != 0) {
        TEST_FAIL("Line 1 extraction failed");
        fl_error_reporter_free(er);
        return;
    }

    if (!line2 || strcmp(line2, "line 2") != 0) {
        TEST_FAIL("Line 2 extraction failed");
        fl_error_reporter_free(er);
        return;
    }

    if (!line3 || strcmp(line3, "line 3") != 0) {
        TEST_FAIL("Line 3 extraction failed");
        fl_error_reporter_free(er);
        return;
    }

    fl_error_reporter_free(er);
    TEST_PASS();
}

/* =================================================================== */
/* Test 19: Error Message Defaults                                    */
/* =================================================================== */
void test_error_message_defaults() {
    TEST_START("Error message defaults");

    const char* msg001 = fl_error_diag_default_msg(FL_DIAG_E001);
    const char* msg002 = fl_error_diag_default_msg(FL_DIAG_E002);
    const char* msg003 = fl_error_diag_default_msg(FL_DIAG_E003);

    if (strcmp(msg001, "undefined variable") != 0) {
        TEST_FAIL("E001 message incorrect");
        return;
    }

    if (strcmp(msg002, "undefined function") != 0) {
        TEST_FAIL("E002 message incorrect");
        return;
    }

    if (strcmp(msg003, "type mismatch") != 0) {
        TEST_FAIL("E003 message incorrect");
        return;
    }

    TEST_PASS();
}

/* =================================================================== */
/* Test 20: Error Code String Format                                  */
/* =================================================================== */
void test_error_code_string_format() {
    TEST_START("Error code string format");

    const char* code001 = fl_error_diag_code_str(FL_DIAG_E001);
    const char* code010 = fl_error_diag_code_str(FL_DIAG_E010);
    const char* code015 = fl_error_diag_code_str(FL_DIAG_E015);

    if (strcmp(code001, "E001") != 0) {
        TEST_FAIL("E001 code string incorrect");
        return;
    }

    if (strcmp(code010, "E010") != 0) {
        TEST_FAIL("E010 code string incorrect");
        return;
    }

    if (strcmp(code015, "E015") != 0) {
        TEST_FAIL("E015 code string incorrect");
        return;
    }

    TEST_PASS();
}

/* =================================================================== */
/* Main Test Runner                                                    */
/* =================================================================== */
int main(void) {
    printf("\n");
    printf("════════════════════════════════════════════════════════════════\n");
    printf("  FreeLang Error Reporter Tests - Phase 2-C\n");
    printf("  Rust Compiler Style Error Messages\n");
    printf("════════════════════════════════════════════════════════════════\n");

    /* Run all tests */
    test_error_reporter_create();
    test_e001_undefined_variable();
    test_e002_undefined_function();
    test_e003_type_mismatch();
    test_e005_invalid_argument_count();
    test_e009_syntax_error();
    test_e006_duplicate_definition();
    test_e013_divide_by_zero();
    test_e014_unreachable_code();
    test_e015_missing_return();
    test_e010_unexpected_token();
    test_e011_unclosed_bracket();
    test_e004_invalid_operator();
    test_e007_invalid_index();
    test_e008_invalid_member_access();
    test_e012_invalid_escape_sequence();
    test_multiple_diagnostics();
    test_source_line_extraction();
    test_error_message_defaults();
    test_error_code_string_format();

    /* Summary */
    printf("\n");
    printf("════════════════════════════════════════════════════════════════\n");
    printf("Test Results:\n");
    printf("  Total:  %d\n", test_count);
    printf("  Passed: %d %s\n", pass_count, ANSI_GREEN "✓" ANSI_RESET);
    printf("  Failed: %d %s\n", fail_count, fail_count > 0 ? ANSI_RED "✗" ANSI_RESET : "");
    printf("════════════════════════════════════════════════════════════════\n\n");

    return fail_count == 0 ? 0 : 1;
}
