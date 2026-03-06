#include "../include/ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
   Global AST Node Pool
   ============================================================================ */

ASTNodePool g_ast_pool = {0};

/* ============================================================================
   Node Type Name Mapping
   ============================================================================ */

static const char *node_type_names[] = {
    "PROGRAM",
    "VAR_DECL",
    "FN_DECL",
    "STRUCT_DECL",
    "ENUM_DECL",
    "IMPORT",
    "EXPORT",
    "BLOCK",
    "IF",
    "WHILE",
    "FOR",
    "FOR_IN",
    "RETURN",
    "BREAK",
    "CONTINUE",
    "TRY",
    "THROW",
    "BINARY",
    "UNARY",
    "LOGICAL",
    "CALL",
    "MEMBER",
    "ASSIGN",
    "TERNARY",
    "ARRAY",
    "OBJECT",
    "PROPERTY",
    "FN_EXPR",
    "ARROW_FN",
    "AWAIT",
    "FSTRING",
    "QUESTION_OP",
    "IDENT",
    "LITERAL",
    "CATCH_CLAUSE",
    "FINALLY_CLAUSE",
};

/* ============================================================================
   Pool Initialization & Management
   ============================================================================ */

void ast_init_pool(size_t capacity)
{
    if (g_ast_pool.nodes != NULL) {
        free(g_ast_pool.nodes);
        free(g_ast_pool.is_used);
    }

    g_ast_pool.capacity = capacity;
    g_ast_pool.count = 0;
    g_ast_pool.nodes = (fl_ast_node_t *)calloc(capacity, sizeof(fl_ast_node_t));
    g_ast_pool.is_used = (int *)calloc(capacity, sizeof(int));

    if (g_ast_pool.nodes == NULL || g_ast_pool.is_used == NULL) {
        fprintf(stderr, "ERROR: Failed to allocate AST pool\n");
        exit(1);
    }
}

/* ============================================================================
   Node Allocation
   ============================================================================ */

fl_ast_node_t *ast_alloc(NodeType type)
{
    if (g_ast_pool.nodes == NULL) {
        ast_init_pool(10000);  /* Default capacity */
    }

    /* Find first free slot */
    for (size_t i = 0; i < g_ast_pool.capacity; i++) {
        if (!g_ast_pool.is_used[i]) {
            g_ast_pool.is_used[i] = 1;
            fl_ast_node_t *node = &g_ast_pool.nodes[i];

            /* Clear memory */
            memset(node, 0, sizeof(fl_ast_node_t));

            /* Set type */
            node->type = type;
            node->line = 0;
            node->col = 0;
            node->type_info = NULL;

            g_ast_pool.count++;
            return node;
        }
    }

    fprintf(stderr, "ERROR: AST pool exhausted (capacity: %zu)\n", g_ast_pool.capacity);
    exit(1);
}

/* ============================================================================
   Node Freeing
   ============================================================================ */

void ast_free(fl_ast_node_t *node)
{
    if (node == NULL) return;

    /* Calculate pool index */
    ptrdiff_t idx = node - g_ast_pool.nodes;

    if (idx >= 0 && idx < (ptrdiff_t)g_ast_pool.capacity) {
        g_ast_pool.is_used[idx] = 0;
        g_ast_pool.count--;
    }
}

void ast_free_all(void)
{
    if (g_ast_pool.nodes != NULL) {
        free(g_ast_pool.nodes);
        free(g_ast_pool.is_used);
    }

    g_ast_pool.nodes = NULL;
    g_ast_pool.is_used = NULL;
    g_ast_pool.capacity = 0;
    g_ast_pool.count = 0;
}

/* ============================================================================
   Node Cloning (Deep Copy)
   ============================================================================ */

fl_ast_node_t *ast_clone(fl_ast_node_t *node)
{
    if (node == NULL) return NULL;

    fl_ast_node_t *cloned = ast_alloc(node->type);
    cloned->line = node->line;
    cloned->col = node->col;

    /* Clone type info if present */
    if (node->type_info != NULL) {
        cloned->type_info = type_annotation_new(
            node->type_info->name,
            node->type_info->is_array,
            node->type_info->is_optional
        );
    }

    /* Clone union data based on node type */
    switch (node->type) {
        case NODE_PROGRAM:
            cloned->data.program.item_count = node->data.program.item_count;
            if (node->data.program.item_count > 0) {
                cloned->data.program.items =
                    (fl_ast_node_t **)malloc(sizeof(fl_ast_node_t *) * node->data.program.item_count);
                for (int i = 0; i < node->data.program.item_count; i++) {
                    cloned->data.program.items[i] = ast_clone(node->data.program.items[i]);
                }
            }
            break;

        case NODE_VAR_DECL:
            strncpy(cloned->data.var_decl.kind, node->data.var_decl.kind, 7);
            strncpy(cloned->data.var_decl.name, node->data.var_decl.name, 255);
            cloned->data.var_decl.init = ast_clone(node->data.var_decl.init);
            break;

        case NODE_IDENT:
            strncpy(cloned->data.ident.name, node->data.ident.name, 255);
            break;

        case NODE_LITERAL:
            cloned->data.literal.num = node->data.literal.num;
            cloned->data.literal.is_bool = node->data.literal.is_bool;
            cloned->data.literal.bool_val = node->data.literal.bool_val;
            cloned->data.literal.is_null = node->data.literal.is_null;
            cloned->data.literal.is_nan = node->data.literal.is_nan;
            cloned->data.literal.is_inf = node->data.literal.is_inf;
            if (node->data.literal.str != NULL) {
                cloned->data.literal.str = (char *)malloc(strlen(node->data.literal.str) + 1);
                strcpy(cloned->data.literal.str, node->data.literal.str);
            }
            break;

        case NODE_BINARY:
            cloned->data.binary.left = ast_clone(node->data.binary.left);
            strncpy(cloned->data.binary.op, node->data.binary.op, 15);
            cloned->data.binary.right = ast_clone(node->data.binary.right);
            break;

        case NODE_UNARY:
            strncpy(cloned->data.unary.op, node->data.unary.op, 15);
            cloned->data.unary.operand = ast_clone(node->data.unary.operand);
            cloned->data.unary.is_prefix = node->data.unary.is_prefix;
            break;

        case NODE_BLOCK:
            cloned->data.block.stmt_count = node->data.block.stmt_count;
            if (node->data.block.stmt_count > 0) {
                cloned->data.block.statements =
                    (fl_ast_node_t **)malloc(sizeof(fl_ast_node_t *) * node->data.block.stmt_count);
                for (int i = 0; i < node->data.block.stmt_count; i++) {
                    cloned->data.block.statements[i] = ast_clone(node->data.block.statements[i]);
                }
            }
            break;

        case NODE_IF:
            cloned->data.if_stmt.test = ast_clone(node->data.if_stmt.test);
            cloned->data.if_stmt.consequent = ast_clone(node->data.if_stmt.consequent);
            cloned->data.if_stmt.alternate = ast_clone(node->data.if_stmt.alternate);
            break;

        case NODE_CALL:
            cloned->data.call.callee = ast_clone(node->data.call.callee);
            cloned->data.call.arg_count = node->data.call.arg_count;
            if (node->data.call.arg_count > 0) {
                cloned->data.call.arguments =
                    (fl_ast_node_t **)malloc(sizeof(fl_ast_node_t *) * node->data.call.arg_count);
                for (int i = 0; i < node->data.call.arg_count; i++) {
                    cloned->data.call.arguments[i] = ast_clone(node->data.call.arguments[i]);
                }
            }
            break;

        case NODE_ARRAY:
            cloned->data.array.element_count = node->data.array.element_count;
            if (node->data.array.element_count > 0) {
                cloned->data.array.elements =
                    (fl_ast_node_t **)malloc(sizeof(fl_ast_node_t *) * node->data.array.element_count);
                for (int i = 0; i < node->data.array.element_count; i++) {
                    cloned->data.array.elements[i] = ast_clone(node->data.array.elements[i]);
                }
            }
            break;

        case NODE_MEMBER:
            cloned->data.member.object = ast_clone(node->data.member.object);
            cloned->data.member.property = ast_clone(node->data.member.property);
            cloned->data.member.is_computed = node->data.member.is_computed;
            break;

        case NODE_ASSIGN:
            cloned->data.assign.target = ast_clone(node->data.assign.target);
            strncpy(cloned->data.assign.op, node->data.assign.op, 7);
            cloned->data.assign.value = ast_clone(node->data.assign.value);
            break;

        /* Add more cases as needed */
        default:
            break;
    }

    return cloned;
}

/* ============================================================================
   Debug Output
   ============================================================================ */

void ast_print(fl_ast_node_t *node, int indent)
{
    if (node == NULL) {
        return;
    }

    char *indent_str = (char *)malloc(indent + 1);
    memset(indent_str, ' ', indent);
    indent_str[indent] = '\0';

    const char *type_name = ast_type_name(node->type);

    switch (node->type) {
        case NODE_IDENT:
            printf("%s[IDENT] %s\n", indent_str, node->data.ident.name);
            break;

        case NODE_LITERAL:
            if (node->data.literal.is_null) {
                printf("%s[LITERAL] null\n", indent_str);
            } else if (node->data.literal.is_bool) {
                printf("%s[LITERAL] %s\n", indent_str, node->data.literal.bool_val ? "true" : "false");
            } else if (node->data.literal.str != NULL) {
                printf("%s[LITERAL] \"%s\"\n", indent_str, node->data.literal.str);
            } else {
                printf("%s[LITERAL] %g\n", indent_str, node->data.literal.num);
            }
            break;

        case NODE_BINARY:
            printf("%s[BINARY] op=%s\n", indent_str, node->data.binary.op);
            printf("%s  left:\n", indent_str);
            ast_print(node->data.binary.left, indent + 4);
            printf("%s  right:\n", indent_str);
            ast_print(node->data.binary.right, indent + 4);
            break;

        case NODE_VAR_DECL:
            printf("%s[VAR_DECL] %s %s\n", indent_str, node->data.var_decl.kind,
                   node->data.var_decl.name);
            if (node->data.var_decl.init != NULL) {
                printf("%s  init:\n", indent_str);
                ast_print(node->data.var_decl.init, indent + 4);
            }
            break;

        case NODE_BLOCK:
            printf("%s[BLOCK] (%d stmts)\n", indent_str, node->data.block.stmt_count);
            for (int i = 0; i < node->data.block.stmt_count; i++) {
                ast_print(node->data.block.statements[i], indent + 2);
            }
            break;

        case NODE_IF:
            printf("%s[IF]\n", indent_str);
            printf("%s  test:\n", indent_str);
            ast_print(node->data.if_stmt.test, indent + 4);
            printf("%s  then:\n", indent_str);
            ast_print(node->data.if_stmt.consequent, indent + 4);
            if (node->data.if_stmt.alternate != NULL) {
                printf("%s  else:\n", indent_str);
                ast_print(node->data.if_stmt.alternate, indent + 4);
            }
            break;

        case NODE_CALL:
            printf("%s[CALL] (%d args)\n", indent_str, node->data.call.arg_count);
            printf("%s  callee:\n", indent_str);
            ast_print(node->data.call.callee, indent + 4);
            printf("%s  args:\n", indent_str);
            for (int i = 0; i < node->data.call.arg_count; i++) {
                ast_print(node->data.call.arguments[i], indent + 4);
            }
            break;

        case NODE_ARRAY:
            printf("%s[ARRAY] (%d elements)\n", indent_str, node->data.array.element_count);
            for (int i = 0; i < node->data.array.element_count; i++) {
                ast_print(node->data.array.elements[i], indent + 2);
            }
            break;

        case NODE_PROGRAM:
            printf("%s[PROGRAM] (%d items)\n", indent_str, node->data.program.item_count);
            for (int i = 0; i < node->data.program.item_count; i++) {
                ast_print(node->data.program.items[i], indent + 2);
            }
            break;

        default:
            printf("%s[%s]\n", indent_str, type_name);
            break;
    }

    free(indent_str);
}

const char *ast_type_name(NodeType type)
{
    if (type >= 0 && type < NODE_TYPE_MAX) {
        return node_type_names[type];
    }
    return "UNKNOWN";
}

/* ============================================================================
   Type Annotation Functions
   ============================================================================ */

TypeAnnotation *type_annotation_new(const char *name, int is_array, int is_optional)
{
    TypeAnnotation *type = (TypeAnnotation *)malloc(sizeof(TypeAnnotation));
    if (type == NULL) {
        fprintf(stderr, "ERROR: Failed to allocate TypeAnnotation\n");
        return NULL;
    }

    strncpy(type->name, name, 127);
    type->name[127] = '\0';
    type->is_array = is_array;
    type->is_optional = is_optional;
    type->generic_arg = NULL;

    return type;
}

void type_annotation_free(TypeAnnotation *type)
{
    if (type != NULL) {
        if (type->generic_arg != NULL) {
            ast_free(type->generic_arg);
        }
        free(type);
    }
}

/* ============================================================================
   Helper Functions for Node Creation
   ============================================================================ */

fl_ast_node_t *ast_new_var_decl(const char *kind, const char *name, fl_ast_node_t *init)
{
    fl_ast_node_t *node = ast_alloc(NODE_VAR_DECL);
    strncpy(node->data.var_decl.kind, kind, 7);
    strncpy(node->data.var_decl.name, name, 255);
    node->data.var_decl.init = init;
    return node;
}

fl_ast_node_t *ast_new_fn_decl(const char *name, char **params, int param_count,
                         fl_ast_node_t *body, int is_async)
{
    fl_ast_node_t *node = ast_alloc(NODE_FN_DECL);
    strncpy(node->data.fn_decl.name, name, 255);
    node->data.fn_decl.param_names = params;
    node->data.fn_decl.param_count = param_count;
    node->data.fn_decl.body = body;
    node->data.fn_decl.is_async = is_async;
    return node;
}

fl_ast_node_t *ast_new_binary(fl_ast_node_t *left, const char *op, fl_ast_node_t *right)
{
    fl_ast_node_t *node = ast_alloc(NODE_BINARY);
    node->data.binary.left = left;
    strncpy(node->data.binary.op, op, 15);
    node->data.binary.right = right;
    return node;
}

fl_ast_node_t *ast_new_ident(const char *name)
{
    fl_ast_node_t *node = ast_alloc(NODE_IDENT);
    strncpy(node->data.ident.name, name, 255);
    return node;
}

fl_ast_node_t *ast_new_number(double value)
{
    fl_ast_node_t *node = ast_alloc(NODE_LITERAL);
    node->data.literal.num = value;
    node->data.literal.is_bool = 0;
    node->data.literal.is_null = 0;
    node->data.literal.is_nan = (value != value);  /* NaN check */
    node->data.literal.is_inf = (value == 1.0/0.0 || value == -1.0/0.0);
    node->data.literal.str = NULL;
    return node;
}

fl_ast_node_t *ast_new_string(const char *value)
{
    fl_ast_node_t *node = ast_alloc(NODE_LITERAL);
    node->data.literal.str = (char *)malloc(strlen(value) + 1);
    strcpy(node->data.literal.str, value);
    node->data.literal.num = 0;
    node->data.literal.is_bool = 0;
    node->data.literal.is_null = 0;
    return node;
}

fl_ast_node_t *ast_new_bool(int value)
{
    fl_ast_node_t *node = ast_alloc(NODE_LITERAL);
    node->data.literal.is_bool = 1;
    node->data.literal.bool_val = (value != 0);
    node->data.literal.str = NULL;
    node->data.literal.is_null = 0;
    return node;
}

fl_ast_node_t *ast_new_null(void)
{
    fl_ast_node_t *node = ast_alloc(NODE_LITERAL);
    node->data.literal.is_null = 1;
    node->data.literal.str = NULL;
    return node;
}

fl_ast_node_t *ast_new_block(fl_ast_node_t **statements, int stmt_count)
{
    fl_ast_node_t *node = ast_alloc(NODE_BLOCK);
    node->data.block.statements = statements;
    node->data.block.stmt_count = stmt_count;
    return node;
}

fl_ast_node_t *ast_new_call(fl_ast_node_t *callee, fl_ast_node_t **args, int arg_count)
{
    fl_ast_node_t *node = ast_alloc(NODE_CALL);
    node->data.call.callee = callee;
    node->data.call.arguments = args;
    node->data.call.arg_count = arg_count;
    return node;
}

fl_ast_node_t *ast_new_array(fl_ast_node_t **elements, int element_count)
{
    fl_ast_node_t *node = ast_alloc(NODE_ARRAY);
    node->data.array.elements = elements;
    node->data.array.element_count = element_count;
    return node;
}

fl_ast_node_t *ast_new_object(fl_ast_node_t **properties, int property_count)
{
    fl_ast_node_t *node = ast_alloc(NODE_OBJECT);
    node->data.object.properties = properties;
    node->data.object.property_count = property_count;
    return node;
}

fl_ast_node_t *ast_new_if(fl_ast_node_t *test, fl_ast_node_t *consequent, fl_ast_node_t *alternate)
{
    fl_ast_node_t *node = ast_alloc(NODE_IF);
    node->data.if_stmt.test = test;
    node->data.if_stmt.consequent = consequent;
    node->data.if_stmt.alternate = alternate;
    return node;
}

fl_ast_node_t *ast_new_member(fl_ast_node_t *object, fl_ast_node_t *property, int is_computed)
{
    fl_ast_node_t *node = ast_alloc(NODE_MEMBER);
    node->data.member.object = object;
    node->data.member.property = property;
    node->data.member.is_computed = is_computed;
    return node;
}

fl_ast_node_t *ast_new_assign(fl_ast_node_t *target, const char *op, fl_ast_node_t *value)
{
    fl_ast_node_t *node = ast_alloc(NODE_ASSIGN);
    node->data.assign.target = target;
    strncpy(node->data.assign.op, op, 7);
    node->data.assign.value = value;
    return node;
}

fl_ast_node_t *ast_new_try(fl_ast_node_t *body, fl_ast_node_t *catch_clause, fl_ast_node_t *finally_clause)
{
    fl_ast_node_t *node = ast_alloc(NODE_TRY);
    node->data.try_stmt.body = body;
    node->data.try_stmt.catch_clause = catch_clause;
    node->data.try_stmt.finally_clause = finally_clause;
    return node;
}

fl_ast_node_t *ast_new_catch_clause(const char *error_var, fl_ast_node_t *handler)
{
    fl_ast_node_t *node = ast_alloc(NODE_CATCH_CLAUSE);
    strncpy(node->data.catch_clause.error_var, error_var, 255);
    node->data.catch_clause.handler = handler;
    return node;
}

fl_ast_node_t *ast_new_finally_clause(fl_ast_node_t *handler)
{
    fl_ast_node_t *node = ast_alloc(NODE_FINALLY_CLAUSE);
    node->data.finally_clause.handler = handler;
    return node;
}

/**
 * Helper: create function expression node fn(params) { body }
 */
fl_ast_node_t *ast_new_fn_expr(char **param_names, int param_count, fl_ast_node_t *body)
{
    fl_ast_node_t *node = ast_alloc(NODE_FN_EXPR);
    node->data.fn_expr.param_names = param_names;
    node->data.fn_expr.param_count = param_count;
    node->data.fn_expr.body = body;
    node->data.fn_expr.param_types = NULL;
    node->data.fn_expr.return_type = NULL;
    return node;
}

/* ============================================================================
   Pool Statistics
   ============================================================================ */

void ast_pool_stats(void)
{
    if (g_ast_pool.nodes == NULL) {
        printf("AST Pool: Not initialized\n");
        return;
    }

    printf("=== AST Pool Statistics ===\n");
    printf("Capacity: %zu\n", g_ast_pool.capacity);
    printf("In Use: %zu\n", g_ast_pool.count);
    printf("Free: %zu\n", g_ast_pool.capacity - g_ast_pool.count);
    printf("Usage: %.1f%%\n", (100.0 * g_ast_pool.count) / g_ast_pool.capacity);
}
