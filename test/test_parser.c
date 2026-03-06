/**
 * Unit tests for Recursive Descent Parser
 * Tests basic parser functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/parser.h"
#include "../include/ast.h"
#include "../include/token.h"

static int test_count = 0;
static int test_passed = 0;

#define TEST_START() do { test_count++; printf("\n[Test %d] ", test_count); } while(0)
#define TEST_PASS() do { test_passed++; printf("PASS\n"); } while(0)
#define TEST_FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

void test_parser_basic(void) {
    TEST_START();
    printf("Parser initialization");

    Token tokens[2];
    tokens[0].type = TOK_LET;
    tokens[0].value = (char*)"let";
    tokens[0].line = 1;
    tokens[0].col = 1;

    tokens[1].type = TOK_EOF;
    tokens[1].value = (char*)"";
    tokens[1].line = 1;
    tokens[1].col = 1;

    fl_parser_t *parser = fl_parser_create(tokens, 2);

    if (!parser) {
        TEST_FAIL("Parser creation failed");
        return;
    }

    fl_parser_destroy(parser);
    TEST_PASS();
}

void test_simple_variable(void) {
    TEST_START();
    printf("Simple variable parsing");

    Token tokens[6];
    int idx = 0;

    tokens[idx].type = TOK_LET;
    tokens[idx].value = (char*)"let";
    tokens[idx].line = 1;
    tokens[idx].col = 1;
    idx++;

    tokens[idx].type = TOK_IDENT;
    tokens[idx].value = (char*)"x";
    tokens[idx].line = 1;
    tokens[idx].col = 5;
    idx++;

    tokens[idx].type = TOK_EQ;
    tokens[idx].value = (char*)"=";
    tokens[idx].line = 1;
    tokens[idx].col = 7;
    idx++;

    tokens[idx].type = TOK_NUMBER;
    tokens[idx].value = (char*)"10";
    tokens[idx].num_value = 10;
    tokens[idx].line = 1;
    tokens[idx].col = 9;
    idx++;

    tokens[idx].type = TOK_SEMICOLON;
    tokens[idx].value = (char*)";";
    tokens[idx].line = 1;
    tokens[idx].col = 11;
    idx++;

    tokens[idx].type = TOK_EOF;
    tokens[idx].value = (char*)"";
    tokens[idx].line = 1;
    tokens[idx].col = 12;
    idx++;

    fl_parser_t *parser = fl_parser_create(tokens, idx);
    if (!parser) {
        TEST_FAIL("Parser creation failed");
        return;
    }

    ASTNode *ast = fl_parser_parse(parser);
    if (!ast || ast->type != NODE_PROGRAM) {
        TEST_FAIL("Expected PROGRAM node");
        fl_parser_destroy(parser);
        return;
    }

    if (ast->data.program.item_count != 1) {
        TEST_FAIL("Expected 1 statement");
        fl_parser_destroy(parser);
        return;
    }

    ASTNode *var_node = ast->data.program.items[0];
    if (var_node->type != NODE_VAR_DECL) {
        TEST_FAIL("Expected VAR_DECL node");
        fl_parser_destroy(parser);
        return;
    }

    if (strcmp(var_node->data.var_decl.name, "x") != 0) {
        TEST_FAIL("Variable name mismatch");
        fl_parser_destroy(parser);
        return;
    }

    if (strcmp(var_node->data.var_decl.kind, "let") != 0) {
        TEST_FAIL("Variable kind mismatch");
        fl_parser_destroy(parser);
        return;
    }

    fl_parser_destroy(parser);
    TEST_PASS();
}

void test_function_decl(void) {
    TEST_START();
    printf("Function declaration parsing");

    Token tokens[15];
    int idx = 0;

    tokens[idx].type = TOK_FN;
    tokens[idx].value = (char*)"fn";
    tokens[idx].line = 1;
    tokens[idx].col = 1;
    idx++;

    tokens[idx].type = TOK_IDENT;
    tokens[idx].value = (char*)"add";
    tokens[idx].line = 1;
    tokens[idx].col = 4;
    idx++;

    tokens[idx].type = TOK_LPAREN;
    tokens[idx].value = (char*)"(";
    tokens[idx].line = 1;
    tokens[idx].col = 7;
    idx++;

    tokens[idx].type = TOK_IDENT;
    tokens[idx].value = (char*)"a";
    tokens[idx].line = 1;
    tokens[idx].col = 8;
    idx++;

    tokens[idx].type = TOK_COMMA;
    tokens[idx].value = (char*)",";
    tokens[idx].line = 1;
    tokens[idx].col = 9;
    idx++;

    tokens[idx].type = TOK_IDENT;
    tokens[idx].value = (char*)"b";
    tokens[idx].line = 1;
    tokens[idx].col = 11;
    idx++;

    tokens[idx].type = TOK_RPAREN;
    tokens[idx].value = (char*)")";
    tokens[idx].line = 1;
    tokens[idx].col = 12;
    idx++;

    tokens[idx].type = TOK_LBRACE;
    tokens[idx].value = (char*)"{";
    tokens[idx].line = 1;
    tokens[idx].col = 14;
    idx++;

    tokens[idx].type = TOK_RBRACE;
    tokens[idx].value = (char*)"}";
    tokens[idx].line = 1;
    tokens[idx].col = 15;
    idx++;

    tokens[idx].type = TOK_EOF;
    tokens[idx].value = (char*)"";
    tokens[idx].line = 1;
    tokens[idx].col = 16;
    idx++;

    fl_parser_t *parser = fl_parser_create(tokens, idx);
    if (!parser) {
        TEST_FAIL("Parser creation failed");
        return;
    }

    ASTNode *ast = fl_parser_parse(parser);
    if (!ast || ast->type != NODE_PROGRAM) {
        TEST_FAIL("Expected PROGRAM node");
        fl_parser_destroy(parser);
        return;
    }

    if (ast->data.program.item_count != 1) {
        TEST_FAIL("Expected 1 statement");
        fl_parser_destroy(parser);
        return;
    }

    ASTNode *fn_node = ast->data.program.items[0];
    if (fn_node->type != NODE_FN_DECL) {
        TEST_FAIL("Expected FN_DECL node");
        fl_parser_destroy(parser);
        return;
    }

    if (strcmp(fn_node->data.fn_decl.name, "add") != 0) {
        TEST_FAIL("Function name mismatch");
        fl_parser_destroy(parser);
        return;
    }

    if (fn_node->data.fn_decl.param_count != 2) {
        TEST_FAIL("Expected 2 parameters");
        fl_parser_destroy(parser);
        return;
    }

    fl_parser_destroy(parser);
    TEST_PASS();
}

void test_if_statement(void) {
    TEST_START();
    printf("If statement parsing");

    Token tokens[10];
    int idx = 0;

    tokens[idx].type = TOK_IF;
    tokens[idx].value = (char*)"if";
    tokens[idx].line = 1;
    tokens[idx].col = 1;
    idx++;

    tokens[idx].type = TOK_LPAREN;
    tokens[idx].value = (char*)"(";
    tokens[idx].line = 1;
    tokens[idx].col = 4;
    idx++;

    tokens[idx].type = TOK_TRUE;
    tokens[idx].value = (char*)"true";
    tokens[idx].line = 1;
    tokens[idx].col = 5;
    idx++;

    tokens[idx].type = TOK_RPAREN;
    tokens[idx].value = (char*)")";
    tokens[idx].line = 1;
    tokens[idx].col = 9;
    idx++;

    tokens[idx].type = TOK_LBRACE;
    tokens[idx].value = (char*)"{";
    tokens[idx].line = 1;
    tokens[idx].col = 11;
    idx++;

    tokens[idx].type = TOK_RBRACE;
    tokens[idx].value = (char*)"}";
    tokens[idx].line = 1;
    tokens[idx].col = 12;
    idx++;

    tokens[idx].type = TOK_EOF;
    tokens[idx].value = (char*)"";
    tokens[idx].line = 1;
    tokens[idx].col = 13;
    idx++;

    fl_parser_t *parser = fl_parser_create(tokens, idx);
    if (!parser) {
        TEST_FAIL("Parser creation failed");
        return;
    }

    ASTNode *ast = fl_parser_parse(parser);
    if (!ast || ast->type != NODE_PROGRAM) {
        TEST_FAIL("Expected PROGRAM node");
        fl_parser_destroy(parser);
        return;
    }

    if (ast->data.program.item_count != 1) {
        TEST_FAIL("Expected 1 statement");
        fl_parser_destroy(parser);
        return;
    }

    ASTNode *if_node = ast->data.program.items[0];
    if (if_node->type != NODE_IF) {
        TEST_FAIL("Expected IF node");
        fl_parser_destroy(parser);
        return;
    }

    fl_parser_destroy(parser);
    TEST_PASS();
}

void test_while_loop(void) {
    TEST_START();
    printf("While loop parsing");

    Token tokens[9];
    int idx = 0;

    tokens[idx].type = TOK_WHILE;
    tokens[idx].value = (char*)"while";
    tokens[idx].line = 1;
    tokens[idx].col = 1;
    idx++;

    tokens[idx].type = TOK_LPAREN;
    tokens[idx].value = (char*)"(";
    tokens[idx].col = 7;
    idx++;

    tokens[idx].type = TOK_TRUE;
    tokens[idx].value = (char*)"true";
    tokens[idx].col = 8;
    idx++;

    tokens[idx].type = TOK_RPAREN;
    tokens[idx].value = (char*)")";
    tokens[idx].col = 12;
    idx++;

    tokens[idx].type = TOK_LBRACE;
    tokens[idx].value = (char*)"{";
    tokens[idx].col = 14;
    idx++;

    tokens[idx].type = TOK_RBRACE;
    tokens[idx].value = (char*)"}";
    tokens[idx].col = 15;
    idx++;

    tokens[idx].type = TOK_EOF;
    tokens[idx].value = (char*)"";
    idx++;

    fl_parser_t *parser = fl_parser_create(tokens, idx);
    ASTNode *ast = fl_parser_parse(parser);

    if (!ast || ast->type != NODE_PROGRAM || ast->data.program.item_count != 1) {
        TEST_FAIL("Parse failed");
        fl_parser_destroy(parser);
        return;
    }

    if (ast->data.program.items[0]->type != NODE_WHILE) {
        TEST_FAIL("Expected WHILE node");
        fl_parser_destroy(parser);
        return;
    }

    fl_parser_destroy(parser);
    TEST_PASS();
}

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("  FreeLang C Parser Test Suite\n");
    printf("========================================\n");

    ast_init_pool(512);

    test_parser_basic();
    test_simple_variable();
    test_function_decl();
    test_if_statement();
    test_while_loop();

    printf("\n");
    printf("========================================\n");
    printf("  Results: %d/%d tests passed\n", test_passed, test_count);
    printf("========================================\n\n");

    return (test_passed == test_count) ? 0 : 1;
}
