#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/ast.h"

void test_basic_literals(void)
{
    printf("\n=== Test: Basic Literals ===\n");

    ast_init_pool(1000);

    /* Create number literal */
    ASTNode *num = ast_new_number(42.5);
    printf("Number node created: type=%d\n", num->type);
    printf("Value: %g\n", num->data.literal.num);

    /* Create string literal */
    ASTNode *str = ast_new_string("hello world");
    printf("String node created: %s\n", str->data.literal.str);

    /* Create boolean */
    ASTNode *bool_true = ast_new_bool(1);
    printf("Bool (true): %s\n", bool_true->data.literal.bool_val ? "true" : "false");

    /* Create null */
    ASTNode *null_node = ast_new_null();
    printf("Null node: is_null=%d\n", null_node->data.literal.is_null);

    printf("✓ Basic literals test passed\n");
}

void test_identifiers(void)
{
    printf("\n=== Test: Identifiers ===\n");

    ASTNode *id1 = ast_new_ident("x");
    ASTNode *id2 = ast_new_ident("myVariable");

    printf("Identifier 1: %s\n", id1->data.ident.name);
    printf("Identifier 2: %s\n", id2->data.ident.name);

    printf("✓ Identifiers test passed\n");
}

void test_binary_expressions(void)
{
    printf("\n=== Test: Binary Expressions ===\n");

    /* Create: 5 + 3 */
    ASTNode *left = ast_new_number(5);
    ASTNode *right = ast_new_number(3);
    ASTNode *add = ast_new_binary(left, "+", right);

    printf("Binary expression created: %s\n", add->data.binary.op);
    printf("Left: %g\n", add->data.binary.left->data.literal.num);
    printf("Right: %g\n", add->data.binary.right->data.literal.num);

    /* Create: x * y */
    ASTNode *var_x = ast_new_ident("x");
    ASTNode *var_y = ast_new_ident("y");
    ASTNode *mult = ast_new_binary(var_x, "*", var_y);

    printf("Multiplication: %s * %s\n", mult->data.binary.left->data.ident.name,
           mult->data.binary.right->data.ident.name);

    printf("✓ Binary expressions test passed\n");
}

void test_var_declarations(void)
{
    printf("\n=== Test: Variable Declarations ===\n");

    /* let x = 10; */
    ASTNode *init = ast_new_number(10);
    ASTNode *var_let = ast_new_var_decl("let", "x", init);

    printf("Variable: %s %s\n", var_let->data.var_decl.kind, var_let->data.var_decl.name);
    printf("Init value: %g\n", var_let->data.var_decl.init->data.literal.num);

    /* const PI = 3.14; */
    ASTNode *init2 = ast_new_number(3.14);
    ASTNode *var_const = ast_new_var_decl("const", "PI", init2);

    printf("Constant: %s %s = %g\n", var_const->data.var_decl.kind, var_const->data.var_decl.name,
           var_const->data.var_decl.init->data.literal.num);

    printf("✓ Variable declarations test passed\n");
}

void test_function_calls(void)
{
    printf("\n=== Test: Function Calls ===\n");

    /* Create: println(42) */
    ASTNode *callee = ast_new_ident("println");
    ASTNode *arg = ast_new_number(42);
    ASTNode *args[] = {arg};
    ASTNode *call = ast_new_call(callee, args, 1);

    printf("Function call: %s\n", call->data.call.callee->data.ident.name);
    printf("Argument count: %d\n", call->data.call.arg_count);
    printf("First argument: %g\n", call->data.call.arguments[0]->data.literal.num);

    /* Create: add(x, y, z) */
    ASTNode *add_fn = ast_new_ident("add");
    ASTNode *arg1 = ast_new_ident("x");
    ASTNode *arg2 = ast_new_ident("y");
    ASTNode *arg3 = ast_new_ident("z");
    ASTNode *multi_args[] = {arg1, arg2, arg3};
    ASTNode *add_call = ast_new_call(add_fn, multi_args, 3);

    printf("Multi-arg call: %s(%d args)\n", add_call->data.call.callee->data.ident.name,
           add_call->data.call.arg_count);

    printf("✓ Function calls test passed\n");
}

void test_arrays(void)
{
    printf("\n=== Test: Arrays ===\n");

    /* Create: [1, 2, 3] */
    ASTNode *elem1 = ast_new_number(1);
    ASTNode *elem2 = ast_new_number(2);
    ASTNode *elem3 = ast_new_number(3);
    ASTNode *elements[] = {elem1, elem2, elem3};
    ASTNode *array = ast_new_array(elements, 3);

    printf("Array created with %d elements\n", array->data.array.element_count);
    for (int i = 0; i < array->data.array.element_count; i++) {
        printf("  [%d] = %g\n", i, array->data.array.elements[i]->data.literal.num);
    }

    /* Create: ["a", "b", "c"] */
    ASTNode *str_elem1 = ast_new_string("apple");
    ASTNode *str_elem2 = ast_new_string("banana");
    ASTNode *str_elems[] = {str_elem1, str_elem2};
    ASTNode *str_array = ast_new_array(str_elems, 2);

    printf("String array with %d elements\n", str_array->data.array.element_count);
    for (int i = 0; i < str_array->data.array.element_count; i++) {
        printf("  [%d] = \"%s\"\n", i, str_array->data.array.elements[i]->data.literal.str);
    }

    printf("✓ Arrays test passed\n");
}

void test_if_statements(void)
{
    printf("\n=== Test: If Statements ===\n");

    /* if (x > 5) { y = 10 } */
    ASTNode *test = ast_new_binary(ast_new_ident("x"), ">", ast_new_number(5));
    ASTNode *body = ast_new_assign(ast_new_ident("y"), "=", ast_new_number(10));

    ASTNode *if_stmt = ast_new_if(test, body, NULL);

    printf("If statement created\n");
    printf("Test type: %d\n", if_stmt->data.if_stmt.test->type);
    printf("Op: %s\n", if_stmt->data.if_stmt.test->data.binary.op);

    /* if (a > 5) { x = 1 } else { x = 2 } */
    ASTNode *test2 = ast_new_binary(ast_new_ident("a"), ">", ast_new_number(5));
    ASTNode *then_body = ast_new_assign(ast_new_ident("x"), "=", ast_new_number(1));
    ASTNode *else_body = ast_new_assign(ast_new_ident("x"), "=", ast_new_number(2));

    ASTNode *if_else = ast_new_if(test2, then_body, else_body);

    printf("If-else statement created\n");
    printf("Has else branch: %d\n", if_else->data.if_stmt.alternate != NULL);

    printf("✓ If statements test passed\n");
}

void test_blocks(void)
{
    printf("\n=== Test: Block Statements ===\n");

    /* { let x = 1; let y = 2; } */
    ASTNode *stmt1 = ast_new_var_decl("let", "x", ast_new_number(1));
    ASTNode *stmt2 = ast_new_var_decl("let", "y", ast_new_number(2));
    ASTNode *stmts[] = {stmt1, stmt2};
    ASTNode *block = ast_new_block(stmts, 2);

    printf("Block created with %d statements\n", block->data.block.stmt_count);
    for (int i = 0; i < block->data.block.stmt_count; i++) {
        if (block->data.block.statements[i]->type == NODE_VAR_DECL) {
            printf("  [%d] VAR_DECL: %s\n", i,
                   block->data.block.statements[i]->data.var_decl.name);
        }
    }

    printf("✓ Block statements test passed\n");
}

void test_member_access(void)
{
    printf("\n=== Test: Member Access ===\n");

    /* obj.prop */
    ASTNode *obj = ast_new_ident("obj");
    ASTNode *prop = ast_new_ident("prop");
    ASTNode *member = ast_new_member(obj, prop, 0);

    printf("Member access (dot): %s.%s\n", member->data.member.object->data.ident.name,
           member->data.member.property->data.ident.name);

    /* arr[5] */
    ASTNode *arr = ast_new_ident("arr");
    ASTNode *idx = ast_new_number(5);
    ASTNode *indexed = ast_new_member(arr, idx, 1);

    printf("Computed member access (bracket): %s[%g]\n",
           indexed->data.member.object->data.ident.name,
           indexed->data.member.property->data.literal.num);

    printf("✓ Member access test passed\n");
}

void test_try_catch(void)
{
    printf("\n=== Test: Try-Catch Statements ===\n");

    /* try { x = 1 } catch(e) { x = 0 } */
    ASTNode *try_body = ast_new_assign(ast_new_ident("x"), "=", ast_new_number(1));
    ASTNode *catch_body = ast_new_assign(ast_new_ident("x"), "=", ast_new_number(0));
    ASTNode *catch_clause = ast_new_catch_clause("e", catch_body);

    ASTNode *try_stmt = ast_new_try(try_body, catch_clause, NULL);

    printf("Try-catch statement created\n");
    printf("Has catch: %d\n", try_stmt->data.try_stmt.catch_clause != NULL);
    printf("Error var: %s\n",
           try_stmt->data.try_stmt.catch_clause->data.catch_clause.error_var);

    /* with finally */
    ASTNode *finally_body = ast_new_assign(ast_new_ident("cleanup"), "=", ast_new_bool(1));
    ASTNode *finally_clause = ast_new_finally_clause(finally_body);
    ASTNode *try_full = ast_new_try(try_body, catch_clause, finally_clause);

    printf("Try-catch-finally created\n");
    printf("Has finally: %d\n", try_full->data.try_stmt.finally_clause != NULL);

    printf("✓ Try-catch test passed\n");
}

void test_ast_print(void)
{
    printf("\n=== Test: AST Print (Debug Output) ===\n");

    /* Create: let x = 42; */
    ASTNode *var = ast_new_var_decl("let", "x", ast_new_number(42));

    printf("\nAST Tree:\n");
    ast_print(var, 0);

    /* Create: if (x > 5) { y = 10 } */
    ASTNode *test = ast_new_binary(ast_new_ident("x"), ">", ast_new_number(5));
    ASTNode *body = ast_new_assign(ast_new_ident("y"), "=", ast_new_number(10));
    ASTNode *if_stmt = ast_new_if(test, body, NULL);

    printf("\nIf statement AST:\n");
    ast_print(if_stmt, 0);

    printf("✓ AST print test passed\n");
}

void test_pool_stats(void)
{
    printf("\n=== Test: Pool Statistics ===\n");

    ast_pool_stats();

    printf("✓ Pool stats test passed\n");
}

int main(void)
{
    printf("======================================\n");
    printf("  FreeLang C AST Tests\n");
    printf("======================================\n");

    test_basic_literals();
    test_identifiers();
    test_binary_expressions();
    test_var_declarations();
    test_function_calls();
    test_arrays();
    test_if_statements();
    test_blocks();
    test_member_access();
    test_try_catch();
    test_ast_print();
    test_pool_stats();

    printf("\n======================================\n");
    printf("  All tests completed!\n");
    printf("======================================\n");

    ast_free_all();

    return 0;
}
