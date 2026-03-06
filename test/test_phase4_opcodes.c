/**
 * Phase 4 VM Opcode Test Suite
 * Tests all arithmetic, comparison, logic, and control flow opcodes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../include/freelang.h"
#include "../include/compiler.h"
#include "../include/vm.h"

/* Test counter */
static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

/* Test assertion macro */
#define TEST(name) do { \
    test_count++; \
    printf("\n[Test %d] %s\n", test_count, name); \
} while(0)

#define ASSERT_INT(actual, expected) do { \
    if ((actual).type == FL_TYPE_INT && (actual).data.int_val == (expected)) { \
        printf("  ✅ PASS: got %ld\n", (actual).data.int_val); \
        pass_count++; \
    } else { \
        printf("  ❌ FAIL: expected %ld, got ", (expected)); \
        if ((actual).type == FL_TYPE_INT) printf("%ld\n", (actual).data.int_val); \
        else if ((actual).type == FL_TYPE_FLOAT) printf("%f\n", (actual).data.float_val); \
        else printf("(non-numeric)\n"); \
        fail_count++; \
    } \
} while(0)

#define ASSERT_FLOAT(actual, expected) do { \
    if ((actual).type == FL_TYPE_FLOAT && \
        fabs((actual).data.float_val - (expected)) < 1e-9) { \
        printf("  ✅ PASS: got %f\n", (actual).data.float_val); \
        pass_count++; \
    } else { \
        printf("  ❌ FAIL: expected %f, got ", (expected)); \
        if ((actual).type == FL_TYPE_FLOAT) printf("%f\n", (actual).data.float_val); \
        else if ((actual).type == FL_TYPE_INT) printf("%ld\n", (actual).data.int_val); \
        else printf("(non-numeric)\n"); \
        fail_count++; \
    } \
} while(0)

#define ASSERT_BOOL(actual, expected) do { \
    if ((actual).type == FL_TYPE_BOOL && (actual).data.bool_val == (expected)) { \
        printf("  ✅ PASS: got %s\n", (expected) ? "true" : "false"); \
        pass_count++; \
    } else { \
        printf("  ❌ FAIL: expected %s\n", (expected) ? "true" : "false"); \
        fail_count++; \
    } \
} while(0)

/* Helper: Create bytecode for simple test */
static Chunk* create_test_chunk(uint8_t *bytecode, size_t bytecode_len,
                                fl_value_t *consts, int const_count) {
    Chunk *chunk = (Chunk*)malloc(sizeof(Chunk));
    chunk->code = bytecode;
    chunk->code_len = bytecode_len;
    chunk->code_capacity = bytecode_len;
    chunk->consts = consts;
    chunk->const_len = const_count;
    chunk->locals_count = 0;
    chunk->line_map = NULL;
    chunk->line_map_capacity = 0;
    return chunk;
}

static void free_test_chunk(Chunk *chunk) {
    if (chunk) free(chunk);
}

/* ========== ARITHMETIC TESTS ========== */

void test_add_int_int(void) {
    TEST("ADD: 5 + 3 = 8 (int)");

    /* Bytecode: PUSH_INT(5) + PUSH_INT(3) + ADD */
    fl_value_t consts[2];
    consts[0].type = FL_TYPE_INT;
    consts[0].data.int_val = 5;
    consts[1].type = FL_TYPE_INT;
    consts[1].data.int_val = 3;

    uint8_t bytecode[] = {
        FL_OP_PUSH_INT,     0, 0, 0, 0,  /* const[0] = 5 */
        FL_OP_PUSH_INT,     0, 0, 0, 1,  /* const[1] = 3 */
        FL_OP_ADD,
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 2);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_INT(result, 8);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

void test_add_string_string(void) {
    TEST("ADD: \"Hello\" + \" World\" (string concat)");

    fl_value_t consts[2];
    consts[0].type = FL_TYPE_STRING;
    consts[0].data.string_val = "Hello";
    consts[1].type = FL_TYPE_STRING;
    consts[1].data.string_val = " World";

    uint8_t bytecode[] = {
        FL_OP_PUSH_STRING,  0, 0, 0, 0,
        FL_OP_PUSH_STRING,  0, 0, 0, 1,
        FL_OP_ADD,
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 2);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);

    if (result.type == FL_TYPE_STRING &&
        strcmp(result.data.string_val, "Hello World") == 0) {
        printf("  ✅ PASS: got \"%s\"\n", result.data.string_val);
        pass_count++;
    } else {
        printf("  ❌ FAIL: string concatenation\n");
        fail_count++;
    }

    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

void test_sub_int(void) {
    TEST("SUB: 10 - 3 = 7 (int)");

    fl_value_t consts[2];
    consts[0].type = FL_TYPE_INT;
    consts[0].data.int_val = 10;
    consts[1].type = FL_TYPE_INT;
    consts[1].data.int_val = 3;

    uint8_t bytecode[] = {
        FL_OP_PUSH_INT,     0, 0, 0, 0,
        FL_OP_PUSH_INT,     0, 0, 0, 1,
        FL_OP_SUB,
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 2);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_INT(result, 7);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

void test_mul_int(void) {
    TEST("MUL: 4 * 5 = 20 (int)");

    fl_value_t consts[2];
    consts[0].type = FL_TYPE_INT;
    consts[0].data.int_val = 4;
    consts[1].type = FL_TYPE_INT;
    consts[1].data.int_val = 5;

    uint8_t bytecode[] = {
        FL_OP_PUSH_INT,     0, 0, 0, 0,
        FL_OP_PUSH_INT,     0, 0, 0, 1,
        FL_OP_MUL,
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 2);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_INT(result, 20);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

void test_div_int(void) {
    TEST("DIV: 20 / 4 = 5 (int)");

    fl_value_t consts[2];
    consts[0].type = FL_TYPE_INT;
    consts[0].data.int_val = 20;
    consts[1].type = FL_TYPE_INT;
    consts[1].data.int_val = 4;

    uint8_t bytecode[] = {
        FL_OP_PUSH_INT,     0, 0, 0, 0,
        FL_OP_PUSH_INT,     0, 0, 0, 1,
        FL_OP_DIV,
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 2);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_INT(result, 5);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

void test_mod_int(void) {
    TEST("MOD: 17 % 5 = 2 (int)");

    fl_value_t consts[2];
    consts[0].type = FL_TYPE_INT;
    consts[0].data.int_val = 17;
    consts[1].type = FL_TYPE_INT;
    consts[1].data.int_val = 5;

    uint8_t bytecode[] = {
        FL_OP_PUSH_INT,     0, 0, 0, 0,
        FL_OP_PUSH_INT,     0, 0, 0, 1,
        FL_OP_MOD,
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 2);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_INT(result, 2);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

void test_float_arithmetic(void) {
    TEST("Float arithmetic: 5.5 + 2.5 = 8.0 (float)");

    fl_value_t consts[2];
    consts[0].type = FL_TYPE_FLOAT;
    consts[0].data.float_val = 5.5;
    consts[1].type = FL_TYPE_FLOAT;
    consts[1].data.float_val = 2.5;

    uint8_t bytecode[] = {
        FL_OP_PUSH_FLOAT,   0, 0, 0, 0,
        FL_OP_PUSH_FLOAT,   0, 0, 0, 1,
        FL_OP_ADD,
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 2);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_FLOAT(result, 8.0);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

/* ========== COMPARISON TESTS ========== */

void test_eq_int_equal(void) {
    TEST("EQ: 5 == 5 = true");

    fl_value_t consts[2];
    consts[0].type = FL_TYPE_INT;
    consts[0].data.int_val = 5;
    consts[1].type = FL_TYPE_INT;
    consts[1].data.int_val = 5;

    uint8_t bytecode[] = {
        FL_OP_PUSH_INT,     0, 0, 0, 0,
        FL_OP_PUSH_INT,     0, 0, 0, 1,
        FL_OP_EQ,
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 2);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_BOOL(result, 1);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

void test_neq_int(void) {
    TEST("NEQ: 3 != 5 = true");

    fl_value_t consts[2];
    consts[0].type = FL_TYPE_INT;
    consts[0].data.int_val = 3;
    consts[1].type = FL_TYPE_INT;
    consts[1].data.int_val = 5;

    uint8_t bytecode[] = {
        FL_OP_PUSH_INT,     0, 0, 0, 0,
        FL_OP_PUSH_INT,     0, 0, 0, 1,
        FL_OP_NEQ,
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 2);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_BOOL(result, 1);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

void test_lt_int(void) {
    TEST("LT: 3 < 5 = true");

    fl_value_t consts[2];
    consts[0].type = FL_TYPE_INT;
    consts[0].data.int_val = 3;
    consts[1].type = FL_TYPE_INT;
    consts[1].data.int_val = 5;

    uint8_t bytecode[] = {
        FL_OP_PUSH_INT,     0, 0, 0, 0,
        FL_OP_PUSH_INT,     0, 0, 0, 1,
        FL_OP_LT,
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 2);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_BOOL(result, 1);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

void test_lte_int(void) {
    TEST("LTE: 5 <= 5 = true");

    fl_value_t consts[2];
    consts[0].type = FL_TYPE_INT;
    consts[0].data.int_val = 5;
    consts[1].type = FL_TYPE_INT;
    consts[1].data.int_val = 5;

    uint8_t bytecode[] = {
        FL_OP_PUSH_INT,     0, 0, 0, 0,
        FL_OP_PUSH_INT,     0, 0, 0, 1,
        FL_OP_LTE,
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 2);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_BOOL(result, 1);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

void test_gt_int(void) {
    TEST("GT: 7 > 3 = true");

    fl_value_t consts[2];
    consts[0].type = FL_TYPE_INT;
    consts[0].data.int_val = 7;
    consts[1].type = FL_TYPE_INT;
    consts[1].data.int_val = 3;

    uint8_t bytecode[] = {
        FL_OP_PUSH_INT,     0, 0, 0, 0,
        FL_OP_PUSH_INT,     0, 0, 0, 1,
        FL_OP_GT,
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 2);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_BOOL(result, 1);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

void test_gte_int(void) {
    TEST("GTE: 5 >= 5 = true");

    fl_value_t consts[2];
    consts[0].type = FL_TYPE_INT;
    consts[0].data.int_val = 5;
    consts[1].type = FL_TYPE_INT;
    consts[1].data.int_val = 5;

    uint8_t bytecode[] = {
        FL_OP_PUSH_INT,     0, 0, 0, 0,
        FL_OP_PUSH_INT,     0, 0, 0, 1,
        FL_OP_GTE,
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 2);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_BOOL(result, 1);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

/* ========== LOGICAL TESTS ========== */

void test_and_true_true(void) {
    TEST("AND: true && true = true");

    fl_value_t consts[2];
    consts[0].type = FL_TYPE_BOOL;
    consts[0].data.bool_val = 1;
    consts[1].type = FL_TYPE_BOOL;
    consts[1].data.bool_val = 1;

    uint8_t bytecode[] = {
        FL_OP_PUSH_BOOL,    0, 0, 0, 1,
        FL_OP_PUSH_BOOL,    0, 0, 0, 1,
        FL_OP_AND,
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 2);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_BOOL(result, 1);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

void test_and_true_false(void) {
    TEST("AND: true && false = false");

    fl_value_t consts[2];
    consts[0].type = FL_TYPE_BOOL;
    consts[0].data.bool_val = 1;
    consts[1].type = FL_TYPE_BOOL;
    consts[1].data.bool_val = 0;

    uint8_t bytecode[] = {
        FL_OP_PUSH_BOOL,    0, 0, 0, 1,
        FL_OP_PUSH_BOOL,    0, 0, 0, 0,
        FL_OP_AND,
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 2);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_BOOL(result, 0);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

void test_or_false_false(void) {
    TEST("OR: false || false = false");

    fl_value_t consts[2];
    consts[0].type = FL_TYPE_BOOL;
    consts[0].data.bool_val = 0;
    consts[1].type = FL_TYPE_BOOL;
    consts[1].data.bool_val = 0;

    uint8_t bytecode[] = {
        FL_OP_PUSH_BOOL,    0, 0, 0, 0,
        FL_OP_PUSH_BOOL,    0, 0, 0, 0,
        FL_OP_OR,
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 2);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_BOOL(result, 0);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

void test_or_true_false(void) {
    TEST("OR: true || false = true");

    fl_value_t consts[2];
    consts[0].type = FL_TYPE_BOOL;
    consts[0].data.bool_val = 1;
    consts[1].type = FL_TYPE_BOOL;
    consts[1].data.bool_val = 0;

    uint8_t bytecode[] = {
        FL_OP_PUSH_BOOL,    0, 0, 0, 1,
        FL_OP_PUSH_BOOL,    0, 0, 0, 0,
        FL_OP_OR,
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 2);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_BOOL(result, 1);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

void test_not_true(void) {
    TEST("NOT: !true = false");

    fl_value_t consts[1];
    consts[0].type = FL_TYPE_BOOL;
    consts[0].data.bool_val = 1;

    uint8_t bytecode[] = {
        FL_OP_PUSH_BOOL,    0, 0, 0, 1,
        FL_OP_NOT,
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 1);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_BOOL(result, 0);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

void test_not_false(void) {
    TEST("NOT: !false = true");

    fl_value_t consts[1];
    consts[0].type = FL_TYPE_BOOL;
    consts[0].data.bool_val = 0;

    uint8_t bytecode[] = {
        FL_OP_PUSH_BOOL,    0, 0, 0, 0,
        FL_OP_NOT,
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 1);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_BOOL(result, 1);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

/* ========== CONTROL FLOW TESTS ========== */

void test_jmp_unconditional(void) {
    TEST("JMP: unconditional jump over instruction");

    fl_value_t consts[2];
    consts[0].type = FL_TYPE_INT;
    consts[0].data.int_val = 1;
    consts[1].type = FL_TYPE_INT;
    consts[1].data.int_val = 2;

    /* Bytecode: PUSH_INT(1) JMP 0x08 PUSH_INT(2) (skipped) RET
       Expected: return 1, NOT 2 */
    uint8_t bytecode[] = {
        FL_OP_PUSH_INT,     0, 0, 0, 0,      /* 0: push 1 */
        FL_OP_JMP,          0, 0, 0, 11,     /* 5: jump to offset 11 (skip next PUSH_INT) */
        FL_OP_PUSH_INT,     0, 0, 0, 1,      /* 10: push 2 (SKIPPED) */
        FL_OP_RET                            /* 15: return */
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 2);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_INT(result, 1);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

void test_jmpt_true_branch(void) {
    TEST("JMPT: jump if true (condition=true)");

    fl_value_t consts[2];
    consts[0].type = FL_TYPE_BOOL;
    consts[0].data.bool_val = 1;
    consts[1].type = FL_TYPE_INT;
    consts[1].data.int_val = 42;

    /* PUSH_BOOL(true) JMPT skip_to PUSH_INT(0) skip_to: PUSH_INT(42) RET
       Expected: 42 */
    uint8_t bytecode[] = {
        FL_OP_PUSH_BOOL,    0, 0, 0, 1,      /* push true */
        FL_OP_JMPT,         0, 0, 0, 10,     /* jump to offset 10 if true */
        FL_OP_PUSH_INT,     0, 0, 0, 0,      /* push 0 (skipped) */
        FL_OP_PUSH_INT,     0, 0, 0, 1,      /* push 42 */
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 2);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_INT(result, 42);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

void test_jmpf_false_branch(void) {
    TEST("JMPF: jump if false (condition=false)");

    fl_value_t consts[2];
    consts[0].type = FL_TYPE_BOOL;
    consts[0].data.bool_val = 0;
    consts[1].type = FL_TYPE_INT;
    consts[1].data.int_val = 99;

    /* PUSH_BOOL(false) JMPF skip_to PUSH_INT(0) skip_to: PUSH_INT(99) RET
       Expected: 99 */
    uint8_t bytecode[] = {
        FL_OP_PUSH_BOOL,    0, 0, 0, 0,      /* push false */
        FL_OP_JMPF,         0, 0, 0, 10,     /* jump to offset 10 if false */
        FL_OP_PUSH_INT,     0, 0, 0, 0,      /* push 0 (skipped) */
        FL_OP_PUSH_INT,     0, 0, 0, 1,      /* push 99 */
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 2);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_INT(result, 99);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

/* ========== COMPLEX EXPRESSION TESTS ========== */

void test_complex_math(void) {
    TEST("Complex: (5 + 3) * 2 = 16");

    fl_value_t consts[3];
    consts[0].type = FL_TYPE_INT;
    consts[0].data.int_val = 5;
    consts[1].type = FL_TYPE_INT;
    consts[1].data.int_val = 3;
    consts[2].type = FL_TYPE_INT;
    consts[2].data.int_val = 2;

    uint8_t bytecode[] = {
        FL_OP_PUSH_INT,     0, 0, 0, 0,      /* push 5 */
        FL_OP_PUSH_INT,     0, 0, 0, 1,      /* push 3 */
        FL_OP_ADD,                            /* 5 + 3 = 8 */
        FL_OP_PUSH_INT,     0, 0, 0, 2,      /* push 2 */
        FL_OP_MUL,                            /* 8 * 2 = 16 */
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 3);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_INT(result, 16);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

void test_complex_logic(void) {
    TEST("Complex: (5 > 3) && (2 < 4) = true");

    fl_value_t consts[4];
    consts[0].type = FL_TYPE_INT;
    consts[0].data.int_val = 5;
    consts[1].type = FL_TYPE_INT;
    consts[1].data.int_val = 3;
    consts[2].type = FL_TYPE_INT;
    consts[2].data.int_val = 2;
    consts[3].type = FL_TYPE_INT;
    consts[3].data.int_val = 4;

    uint8_t bytecode[] = {
        FL_OP_PUSH_INT,     0, 0, 0, 0,      /* push 5 */
        FL_OP_PUSH_INT,     0, 0, 0, 1,      /* push 3 */
        FL_OP_GT,                             /* 5 > 3 = true */
        FL_OP_PUSH_INT,     0, 0, 0, 2,      /* push 2 */
        FL_OP_PUSH_INT,     0, 0, 0, 3,      /* push 4 */
        FL_OP_LT,                             /* 2 < 4 = true */
        FL_OP_AND,                            /* true && true = true */
        FL_OP_RET
    };

    Chunk *chunk = create_test_chunk(bytecode, sizeof(bytecode), consts, 4);
    fl_vm_t *vm = fl_vm_create();
    fl_value_t result = fl_vm_execute(vm, chunk);
    ASSERT_BOOL(result, 1);
    fl_vm_destroy(vm);
    free_test_chunk(chunk);
}

/* ========== MAIN TEST RUNNER ========== */

int main(void) {
    printf("============================================================\n");
    printf("  FreeLang C - Phase 4 VM Opcode Test Suite\n");
    printf("  Testing arithmetic, comparison, logic, and control flow\n");
    printf("============================================================\n");

    /* Arithmetic tests */
    printf("\n📊 ARITHMETIC TESTS\n");
    test_add_int_int();
    test_add_string_string();
    test_sub_int();
    test_mul_int();
    test_div_int();
    test_mod_int();
    test_float_arithmetic();

    /* Comparison tests */
    printf("\n📊 COMPARISON TESTS\n");
    test_eq_int_equal();
    test_neq_int();
    test_lt_int();
    test_lte_int();
    test_gt_int();
    test_gte_int();

    /* Logical tests */
    printf("\n📊 LOGICAL TESTS\n");
    test_and_true_true();
    test_and_true_false();
    test_or_false_false();
    test_or_true_false();
    test_not_true();
    test_not_false();

    /* Control flow tests */
    printf("\n📊 CONTROL FLOW TESTS\n");
    test_jmp_unconditional();
    test_jmpt_true_branch();
    test_jmpf_false_branch();

    /* Complex expression tests */
    printf("\n📊 COMPLEX EXPRESSION TESTS\n");
    test_complex_math();
    test_complex_logic();

    /* Summary */
    printf("\n============================================================\n");
    printf("  TEST RESULTS\n");
    printf("  Total: %d | Passed: %d | Failed: %d\n",
           test_count, pass_count, fail_count);
    printf("============================================================\n");

    return (fail_count > 0) ? 1 : 0;
}
