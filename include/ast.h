#ifndef FREELANG_AST_H
#define FREELANG_AST_H

#include <stddef.h>
#include <stdint.h>

/* ============================================================================
   AST Node Type Enumeration
   ============================================================================ */

typedef enum {
    /* Program & Module */
    NODE_PROGRAM,

    /* Declarations */
    NODE_VAR_DECL,
    NODE_FN_DECL,
    NODE_STRUCT_DECL,
    NODE_ENUM_DECL,

    /* Module System */
    NODE_IMPORT,
    NODE_EXPORT,

    /* Statements */
    NODE_BLOCK,
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,
    NODE_FOR_IN,
    NODE_RETURN,
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_TRY,
    NODE_THROW,

    /* Expressions */
    NODE_BINARY,
    NODE_UNARY,
    NODE_LOGICAL,
    NODE_CALL,
    NODE_MEMBER,
    NODE_ASSIGN,
    NODE_TERNARY,

    /* Literals & Collections */
    NODE_ARRAY,
    NODE_OBJECT,
    NODE_PROPERTY,

    /* Advanced Expressions */
    NODE_FN_EXPR,           /* Function expression: let f = fn(x) { ... } */
    NODE_ARROW_FN,
    NODE_AWAIT,
    NODE_FSTRING,
    NODE_QUESTION_OP,

    /* Identifiers & Literals */
    NODE_IDENT,
    NODE_LITERAL,

    /* Catch Clause (for try-catch) */
    NODE_CATCH_CLAUSE,
    NODE_FINALLY_CLAUSE,

    NODE_TYPE_MAX
} NodeType;

/* ============================================================================
   Type Annotation Structure
   ============================================================================ */

typedef struct {
    char name[128];      /* "number", "string", "bool", "array", "generic" */
    int is_array;        /* 1 if array type */
    int is_optional;     /* 1 if nullable/optional */
    struct fl_ast_node *generic_arg;  /* For array<T>, generic<T>, etc */
} TypeAnnotation;

/* ============================================================================
   Forward Declaration
   ============================================================================ */

/* ============================================================================
   AST Node Union Structure (600+ bytes)
   ============================================================================ */

typedef struct fl_ast_node {
    NodeType type;
    int line;
    int col;
    TypeAnnotation *type_info;  /* Optional type annotation */

    /* Union of all node-specific data */
    union {
#define u data  /* Alias for compatibility */
        /* ================================================================
           NODE_PROGRAM: root node
           ================================================================ */
        struct {
            struct fl_ast_node **items;  /* statements & declarations */
            int item_count;
        } program;

        /* ================================================================
           NODE_VAR_DECL: let/const/var x = init;
           ================================================================ */
        struct {
            char kind[8];           /* "let", "const", "var" */
            char name[256];
            struct fl_ast_node *init;   /* nullable */
            TypeAnnotation *type;   /* optional type hint */
        } var_decl;

        /* ================================================================
           NODE_FN_DECL: fn name(params) -> Type { body }
           ================================================================ */
        struct {
            char name[256];
            char **param_names;     /* array of param names */
            int param_count;
            TypeAnnotation **param_types;  /* optional param types */
            TypeAnnotation *return_type;   /* optional return type */
            struct fl_ast_node *body;   /* BlockStatement */
            int is_async;           /* 1 if async fn */
            int is_generic;         /* 1 if fn<T>(...) */
            char **generic_params;  /* for fn<T, U>(...) */
            int generic_param_count;
        } fn_decl;

        /* ================================================================
           NODE_STRUCT_DECL: struct Name { field1: Type1, ... }
           ================================================================ */
        struct {
            char name[256];
            char **field_names;     /* struct field names */
            TypeAnnotation **field_types;
            int field_count;
        } struct_decl;

        /* ================================================================
           NODE_ENUM_DECL: enum Name { VAR1, VAR2, ... }
           ================================================================ */
        struct {
            char name[256];
            char **variant_names;   /* enum variants */
            int variant_count;
        } enum_decl;

        /* ================================================================
           NODE_IMPORT: import x from "module";
           ================================================================ */
        struct {
            char **imported_names;  /* imported identifiers */
            int import_count;
            char module_path[512];  /* module path/URL */
        } import;

        /* ================================================================
           NODE_EXPORT: export let x = 10;
           ================================================================ */
        struct {
            struct fl_ast_node *declaration;  /* VAR_DECL or FN_DECL */
            int is_default;               /* 1 if export default */
        } export;

        /* ================================================================
           NODE_BLOCK: { stmt1; stmt2; ... }
           ================================================================ */
        struct {
            struct fl_ast_node **statements;
            int stmt_count;
        } block;

        /* ================================================================
           NODE_IF: if (test) consequent else alternate
           ================================================================ */
        struct {
            struct fl_ast_node *test;           /* condition */
            struct fl_ast_node *consequent;     /* then branch */
            struct fl_ast_node *alternate;      /* else branch (nullable) */
        } if_stmt;

        /* ================================================================
           NODE_WHILE: while (test) body
           ================================================================ */
        struct {
            struct fl_ast_node *test;
            struct fl_ast_node *body;
        } while_stmt;

        /* ================================================================
           NODE_FOR: for (init; test; update) body
           ================================================================ */
        struct {
            struct fl_ast_node *init;    /* nullable */
            struct fl_ast_node *test;    /* nullable */
            struct fl_ast_node *update;  /* nullable */
            struct fl_ast_node *body;
        } for_stmt;

        /* ================================================================
           NODE_FOR_IN: for (x in array) body
           ================================================================ */
        struct {
            char var_name[256];      /* loop variable */
            struct fl_ast_node *iterable;
            struct fl_ast_node *body;
        } for_in_stmt;

        /* ================================================================
           NODE_RETURN: return value;
           ================================================================ */
        struct {
            struct fl_ast_node *value;   /* nullable */
        } return_stmt;

        /* ================================================================
           NODE_BREAK: break;
           ================================================================ */
        struct {
            int label;  /* label if labeled break (future use) */
        } break_stmt;

        /* ================================================================
           NODE_CONTINUE: continue;
           ================================================================ */
        struct {
            int label;  /* label if labeled continue (future use) */
        } continue_stmt;

        /* ================================================================
           NODE_TRY: try { body } catch (e) { ... } finally { ... }
           ================================================================ */
        struct {
            struct fl_ast_node *body;              /* try block */
            struct fl_ast_node *catch_clause;      /* NODE_CATCH_CLAUSE (nullable) */
            struct fl_ast_node *finally_clause;    /* NODE_FINALLY_CLAUSE (nullable) */
        } try_stmt;

        /* ================================================================
           NODE_CATCH_CLAUSE: catch (error) { handler }
           ================================================================ */
        struct {
            char error_var[256];    /* "error", "e", etc */
            struct fl_ast_node *handler;
        } catch_clause;

        /* ================================================================
           NODE_FINALLY_CLAUSE: finally { handler }
           ================================================================ */
        struct {
            struct fl_ast_node *handler;
        } finally_clause;

        /* ================================================================
           NODE_THROW: throw error;
           ================================================================ */
        struct {
            struct fl_ast_node *argument;
        } throw_stmt;

        /* ================================================================
           NODE_BINARY: left op right
           ================================================================ */
        struct {
            struct fl_ast_node *left;
            char op[16];  /* "+", "-", "*", "/", "==", "!=", etc */
            struct fl_ast_node *right;
        } binary;

        /* ================================================================
           NODE_UNARY: op operand
           ================================================================ */
        struct {
            char op[16];  /* "-", "!", "~", "++", "--" */
            struct fl_ast_node *operand;
            int is_prefix;  /* 1 if prefix (++x), 0 if postfix (x++) */
        } unary;

        /* ================================================================
           NODE_LOGICAL: left && right, left || right
           ================================================================ */
        struct {
            struct fl_ast_node *left;
            char op[8];  /* "&&", "||" */
            struct fl_ast_node *right;
        } logical;

        /* ================================================================
           NODE_CALL: callee(arg1, arg2, ...)
           ================================================================ */
        struct {
            struct fl_ast_node *callee;
            struct fl_ast_node **arguments;
            int arg_count;
        } call;

        /* ================================================================
           NODE_MEMBER: obj.prop or obj[index]
           ================================================================ */
        struct {
            struct fl_ast_node *object;
            struct fl_ast_node *property;  /* IDENT or LITERAL */
            int is_computed;  /* 1 if obj[prop], 0 if obj.prop */
        } member;

        /* ================================================================
           NODE_ASSIGN: target = value
           ================================================================ */
        struct {
            struct fl_ast_node *target;   /* IDENT or MEMBER */
            char op[8];  /* "=", "+=", "-=", "*=", "/=", etc */
            struct fl_ast_node *value;
        } assign;

        /* ================================================================
           NODE_TERNARY: test ? consequent : alternate
           ================================================================ */
        struct {
            struct fl_ast_node *test;
            struct fl_ast_node *consequent;
            struct fl_ast_node *alternate;
        } ternary;

        /* ================================================================
           NODE_ARRAY: [elem1, elem2, ...]
           ================================================================ */
        struct {
            struct fl_ast_node **elements;
            int element_count;
        } array;

        /* ================================================================
           NODE_OBJECT: { key1: value1, key2: value2, ... }
           ================================================================ */
        struct {
            struct fl_ast_node **properties;  /* array of NODE_PROPERTY */
            int property_count;
        } object;

        /* ================================================================
           NODE_PROPERTY: key: value (used in object literals)
           ================================================================ */
        struct {
            char key[256];
            struct fl_ast_node *value;
            int is_computed;  /* 1 if [key]: value */
        } property;

        /* ================================================================
           NODE_FN_EXPR: fn(params) { body }
           ================================================================ */
        struct {
            char **param_names;
            int param_count;
            struct fl_ast_node *body;   /* BlockStatement */
            TypeAnnotation **param_types;  /* optional param types */
            TypeAnnotation *return_type;   /* optional return type */
        } fn_expr;

        /* ================================================================
           NODE_ARROW_FN: (params) => body
           ================================================================ */
        struct {
            char **param_names;
            int param_count;
            struct fl_ast_node *body;   /* can be expression or block */
            int is_expression;      /* 1 if body is expr (no {}) */
        } arrow_fn;

        /* ================================================================
           NODE_AWAIT: await promise
           ================================================================ */
        struct {
            struct fl_ast_node *argument;
        } await_expr;

        /* ================================================================
           NODE_FSTRING: `Hello ${name}`
           ================================================================ */
        struct {
            char **parts;      /* literal string parts */
            int part_count;
            struct fl_ast_node **expressions;  /* interpolated expressions */
            int expr_count;
        } fstring;

        /* ================================================================
           NODE_QUESTION_OP: obj?.prop or obj?.[index]
           ================================================================ */
        struct {
            struct fl_ast_node *object;
            struct fl_ast_node *property;
            int is_computed;  /* 1 if obj?.[prop] */
        } question_op;

        /* ================================================================
           NODE_IDENT: identifier (variable, function name, etc)
           ================================================================ */
        struct {
            char name[256];
        } ident;

        /* ================================================================
           NODE_LITERAL: literals (number, string, boolean, null)
           ================================================================ */
        struct {
            double num;         /* for numeric literals */
            char *str;          /* for string literals (allocated) */
            int is_bool;        /* 1 if boolean */
            int bool_val;       /* 0 or 1 */
            int is_null;        /* 1 if null */
            int is_nan;         /* 1 if NaN */
            int is_inf;         /* 1 if Infinity */
        } literal;

    } data;

} fl_ast_node_t;

/* ============================================================================
   Object Pool Allocator (for efficient memory management)
   ============================================================================ */

typedef struct {
    fl_ast_node_t *nodes;
    int *is_used;      /* 0 = free, 1 = in use */
    size_t capacity;
    size_t count;
} ASTNodePool;

extern ASTNodePool g_ast_pool;

/* ============================================================================
   Public API Functions
   ============================================================================ */

/**
 * Initialize AST node pool with given capacity
 */
void ast_init_pool(size_t capacity);

/**
 * Allocate a new AST node of given type from pool
 */
fl_ast_node_t *ast_alloc(NodeType type);

/**
 * Free all allocated nodes in pool
 */
void ast_free_all(void);

/**
 * Free a single node (returns to pool)
 */
void ast_free(fl_ast_node_t *node);

/**
 * Clone an AST node (deep copy)
 */
fl_ast_node_t *ast_clone(fl_ast_node_t *node);

/**
 * Print AST node for debugging (recursive)
 * indent: indentation level (0 for root)
 */
void ast_print(fl_ast_node_t *node, int indent);

/**
 * Get node type name as string
 */
const char *ast_type_name(NodeType type);

/**
 * Create type annotation
 */
TypeAnnotation *type_annotation_new(const char *name, int is_array, int is_optional);

/**
 * Free type annotation
 */
void type_annotation_free(TypeAnnotation *type);

/**
 * Helper: create var declaration node
 */
fl_ast_node_t *ast_new_var_decl(const char *kind, const char *name, fl_ast_node_t *init);

/**
 * Helper: create function declaration node
 */
fl_ast_node_t *ast_new_fn_decl(const char *name, char **params, int param_count,
                         fl_ast_node_t *body, int is_async);

/**
 * Helper: create binary expression node
 */
fl_ast_node_t *ast_new_binary(fl_ast_node_t *left, const char *op, fl_ast_node_t *right);

/**
 * Helper: create identifier node
 */
fl_ast_node_t *ast_new_ident(const char *name);

/**
 * Helper: create numeric literal node
 */
fl_ast_node_t *ast_new_number(double value);

/**
 * Helper: create string literal node
 */
fl_ast_node_t *ast_new_string(const char *value);

/**
 * Helper: create boolean literal node
 */
fl_ast_node_t *ast_new_bool(int value);

/**
 * Helper: create null literal node
 */
fl_ast_node_t *ast_new_null(void);

/**
 * Helper: create block statement node
 */
fl_ast_node_t *ast_new_block(fl_ast_node_t **statements, int stmt_count);

/**
 * Helper: create function call node
 */
fl_ast_node_t *ast_new_call(fl_ast_node_t *callee, fl_ast_node_t **args, int arg_count);

/**
 * Helper: create array literal node
 */
fl_ast_node_t *ast_new_array(fl_ast_node_t **elements, int element_count);

/**
 * Helper: create object literal node
 */
fl_ast_node_t *ast_new_object(fl_ast_node_t **properties, int property_count);

/**
 * Helper: create if statement node
 */
fl_ast_node_t *ast_new_if(fl_ast_node_t *test, fl_ast_node_t *consequent, fl_ast_node_t *alternate);

/**
 * Helper: create member access node (obj.prop or obj[idx])
 */
fl_ast_node_t *ast_new_member(fl_ast_node_t *object, fl_ast_node_t *property, int is_computed);

/**
 * Helper: create assignment node
 */
fl_ast_node_t *ast_new_assign(fl_ast_node_t *target, const char *op, fl_ast_node_t *value);

/**
 * Helper: create try-catch statement
 */
fl_ast_node_t *ast_new_try(fl_ast_node_t *body, fl_ast_node_t *catch_clause, fl_ast_node_t *finally_clause);

/**
 * Helper: create catch clause
 */
fl_ast_node_t *ast_new_catch_clause(const char *error_var, fl_ast_node_t *handler);

/**
 * Helper: create finally clause
 */
fl_ast_node_t *ast_new_finally_clause(fl_ast_node_t *handler);

/**
 * Helper: create function expression node fn(params) { body }
 */
fl_ast_node_t *ast_new_fn_expr(char **param_names, int param_count, fl_ast_node_t *body);

/**
 * Get pool statistics (for debugging)
 */
void ast_pool_stats(void);

#endif /* FREELANG_AST_H */
