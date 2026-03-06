/**
 * FreeLang C - Type Checker & Symbol Table
 *
 * 타입 체킹 시스템과 심볼 테이블 관리
 * - Type inference and checking
 * - Symbol table with scope chain
 * - Generic type support
 * - Error reporting
 */

#ifndef FREELANG_TYPECHECKER_H
#define FREELANG_TYPECHECKER_H

#include <stddef.h>
#include <stdint.h>

#include "ast.h"

/* Forward declaration for ASTNode compatibility */
typedef fl_ast_node_t ASTNode;
#define struct_ASTNode fl_ast_node_t

/* ============================================================================
   Type Kind Enumeration
   ============================================================================ */

typedef enum {
    TYPE_NULL,
    TYPE_BOOL,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_ARRAY,      /* array<T> */
    TYPE_OBJECT,     /* {key: T, ...} */
    TYPE_FUNCTION,   /* (T, T) -> T */
    TYPE_GENERIC,    /* <T> */
    TYPE_UNION,      /* T | U */
    TYPE_UNKNOWN,    /* type inference needed */
    TYPE_VOID        /* function return type */
} TypeKind;

/* ============================================================================
   TypeInfo Structure
   ============================================================================ */

/**
 * TypeInfo - Type information descriptor
 *
 * Stores complete type information including:
 * - Basic type kind (int, string, array, etc)
 * - Element type for containers
 * - Parameter/return types for functions
 * - Generic type arguments
 * - Nullability
 */
typedef struct TypeInfo {
    TypeKind kind;
    char name[128];           /* Type name: "int", "array", "Point", etc */

    /* For composite types */
    struct TypeInfo *element_type;    /* array<T> -> T, or null for basic types */
    struct TypeInfo *return_type;     /* function return type */
    struct TypeInfo **param_types;    /* function parameter types */
    int param_count;                  /* number of parameters */

    /* For generic types */
    struct TypeInfo *generic_arg;     /* Generic<T> -> T */

    /* For unions */
    struct TypeInfo **union_types;    /* T1 | T2 | ... */
    int union_count;

    /* Flags */
    int is_nullable;                  /* Can be null/undefined */
    int is_builtin;                   /* Built-in type vs user-defined */
} TypeInfo;

/* ============================================================================
   Symbol Structure
   ============================================================================ */

/**
 * Symbol - Symbol table entry
 *
 * Represents a named entity (variable, function, type)
 * Stores type information and definition location
 */
typedef struct Symbol {
    char name[128];
    TypeInfo *type;
    int is_defined;        /* 1 if let/var/const defined */
    int is_function;       /* 1 if function symbol */
    int is_type;           /* 1 if type symbol (struct/enum) */
    int is_const;          /* 1 if const */
    int line;              /* Definition line number */
    int col;               /* Definition column number */
    void *extra_data;      /* For AST node reference */
} Symbol;

/* ============================================================================
   Symbol Table (Single Scope)
   ============================================================================ */

/**
 * SymbolTable - Single scope level
 *
 * Contains all symbols in one scope
 * Linked to parent scope for chain lookup
 */
typedef struct SymbolTable {
    void *symbols;         /* HashMap<string, Symbol*> */
    struct SymbolTable *parent;  /* Parent scope, NULL for global */
    int depth;             /* Scope depth (0 = global) */
} SymbolTable;

/* ============================================================================
   Scope Manager
   ============================================================================ */

/**
 * Scope - Scope chain manager
 *
 * Manages nested scopes (global, function, block)
 * Provides scope push/pop and symbol lookup across scope chain
 */
typedef struct {
    SymbolTable *current;     /* Current scope */
    void *scope_stack;        /* Vector<SymbolTable*> - all scopes */
} Scope;

/* ============================================================================
   Type Checker
   ============================================================================ */

/**
 * TypeChecker - Main type checking context
 *
 * Manages:
 * - Type inference
 * - Symbol table with scope chain
 * - Type checking and validation
 * - Error reporting
 */
typedef struct {
    Scope *scope;
    void *error_list;        /* Vector<TypeCheckError> */
    int error_count;
    TypeInfo *current_fn_return;  /* Return type of current function */
} TypeChecker;

/* ============================================================================
   Type Error Structure
   ============================================================================ */

typedef struct {
    int line;
    int col;
    char message[512];
} TypeCheckError;

/* ============================================================================
   Constructor/Destructor Functions
   ============================================================================ */

/**
 * typechecker_new - Create new type checker instance
 *
 * Returns: New TypeChecker pointer, NULL on failure
 */
TypeChecker *typechecker_new(void);

/**
 * typechecker_free - Free type checker and all resources
 */
void typechecker_free(TypeChecker *tc);

/**
 * type_new - Create new type descriptor
 * @kind: Type kind (TYPE_INT, TYPE_STRING, etc)
 * @name: Optional type name
 *
 * Returns: New TypeInfo pointer, NULL on failure
 */
TypeInfo *type_new(TypeKind kind, const char *name);

/**
 * type_new_array - Create array type
 * @element_type: Element type (must not be NULL)
 *
 * Returns: array<element_type> descriptor
 * Note: Takes ownership of element_type
 */
TypeInfo *type_new_array(TypeInfo *element_type);

/**
 * type_new_function - Create function type
 * @return_type: Function return type
 * @param_types: Array of parameter types
 * @param_count: Number of parameters
 *
 * Returns: Function type descriptor
 * Note: Takes ownership of return_type and param_types
 */
TypeInfo *type_new_function(TypeInfo *return_type,
                            TypeInfo **param_types, int param_count);

/**
 * type_new_generic - Create generic type
 * @name: Generic name (e.g., "T")
 * @arg: Type argument
 *
 * Returns: Generic<T> descriptor
 * Note: Takes ownership of arg
 */
TypeInfo *type_new_generic(const char *name, TypeInfo *arg);

/**
 * type_new_union - Create union type
 * @types: Array of union member types
 * @count: Number of types
 *
 * Returns: T1 | T2 | ... descriptor
 * Note: Takes ownership of types array
 */
TypeInfo *type_new_union(TypeInfo **types, int count);

/**
 * type_free - Free type descriptor
 *
 * Recursively frees all nested types
 */
void type_free(TypeInfo *type);

/**
 * symbol_new - Create new symbol entry
 * @name: Symbol name
 * @type: Type descriptor (ownership transferred)
 *
 * Returns: New Symbol pointer
 */
Symbol *symbol_new(const char *name, TypeInfo *type);

/**
 * symbol_free - Free symbol entry
 */
void symbol_free(Symbol *sym);

/* ============================================================================
   Scope Management Functions
   ============================================================================ */

/**
 * typechecker_push_scope - Enter new scope
 *
 * Creates new scope level (function, block, etc)
 * Current scope becomes parent of new scope
 */
void typechecker_push_scope(TypeChecker *tc);

/**
 * typechecker_pop_scope - Exit current scope
 *
 * Pops scope and frees symbols
 * Returns to parent scope
 *
 * Returns: 0 on success, -1 if no parent scope
 */
int typechecker_pop_scope(TypeChecker *tc);

/**
 * typechecker_scope_depth - Get current scope depth
 *
 * Returns: 0 for global, 1+ for nested scopes
 */
int typechecker_scope_depth(TypeChecker *tc);

/* ============================================================================
   Symbol Definition & Lookup Functions
   ============================================================================ */

/**
 * typechecker_define - Define new symbol in current scope
 * @name: Symbol name
 * @type: Type descriptor (ownership transferred)
 *
 * Returns: 0 on success, -1 if already defined in current scope
 *
 * Note: Only checks current scope for duplicates (shadowing allowed)
 */
int typechecker_define(TypeChecker *tc, const char *name, TypeInfo *type);

/**
 * typechecker_define_function - Define function symbol
 * @name: Function name
 * @return_type: Return type
 * @param_types: Parameter types
 * @param_count: Number of parameters
 *
 * Returns: 0 on success, -1 on failure
 */
int typechecker_define_function(TypeChecker *tc, const char *name,
                                TypeInfo *return_type,
                                TypeInfo **param_types, int param_count);

/**
 * typechecker_define_type - Define user-defined type (struct/enum)
 * @name: Type name
 *
 * Returns: 0 on success, -1 if already defined
 */
int typechecker_define_type(TypeChecker *tc, const char *name);

/**
 * typechecker_lookup - Look up symbol in scope chain
 * @name: Symbol name
 *
 * Returns: Symbol pointer if found, NULL otherwise
 * Searches current scope, then parent scopes recursively
 */
Symbol *typechecker_lookup(TypeChecker *tc, const char *name);

/**
 * typechecker_lookup_current - Look up in current scope only
 * @name: Symbol name
 *
 * Returns: Symbol pointer if found in current scope only
 */
Symbol *typechecker_lookup_current(TypeChecker *tc, const char *name);

/**
 * typechecker_is_defined - Check if symbol is defined
 * @name: Symbol name
 *
 * Returns: 1 if defined, 0 otherwise
 */
int typechecker_is_defined(TypeChecker *tc, const char *name);

/* ============================================================================
   Type Checking Functions
   ============================================================================ */

/**
 * type_equal - Check type equality
 * @t1, @t2: Types to compare
 *
 * Returns: 1 if types are equal, 0 otherwise
 *
 * Compares:
 * - Kind (TYPE_INT, TYPE_STRING, etc)
 * - Element types for containers
 * - Parameter and return types for functions
 */
int type_equal(TypeInfo *t1, TypeInfo *t2);

/**
 * type_compatible - Check if types are compatible
 * @from: Source type (what we have)
 * @to: Target type (what we need)
 *
 * Returns: 1 if from can be implicitly converted to to, 0 otherwise
 *
 * Allows:
 * - Same type
 * - int -> float (implicit)
 * - null -> nullable types
 * - Subtype compatibility (future)
 */
int type_compatible(TypeInfo *from, TypeInfo *to);

/**
 * type_to_string - Convert type to readable string
 * @type: Type descriptor
 * @buf: Output buffer
 * @size: Buffer size
 *
 * Returns: buf
 *
 * Examples:
 * - "int"
 * - "array<string>"
 * - "(int, string) -> bool"
 * - "T"
 * - "int | string"
 */
char *type_to_string(TypeInfo *type, char *buf, size_t size);

/**
 * infer_type - Infer type of AST expression
 * @tc: Type checker context
 * @node: AST node
 *
 * Returns: Inferred type descriptor, NULL on error
 *
 * Handles:
 * - Literals (int, string, bool, etc)
 * - Identifiers (variable lookup)
 * - Binary operations (arithmetic, comparison, etc)
 * - Function calls
 * - Array/object construction
 * - Member access
 */
TypeInfo *infer_type(TypeChecker *tc, ASTNode *node);

/**
 * check_binary_op - Type check binary operation
 * @left: Left operand type
 * @op: Operator string ("+", "-", "==", etc)
 * @right: Right operand type
 *
 * Returns: Result type, NULL if operation invalid
 *
 * Examples:
 * - int + int -> int
 * - int + float -> float
 * - string + string -> string
 * - int == int -> bool
 * - int < int -> bool
 */
TypeInfo *check_binary_op(TypeInfo *left, const char *op, TypeInfo *right);

/**
 * check_unary_op - Type check unary operation
 * @op: Operator string ("-", "!", etc)
 * @operand: Operand type
 *
 * Returns: Result type, NULL if operation invalid
 */
TypeInfo *check_unary_op(const char *op, TypeInfo *operand);

/**
 * check_function_call - Type check function call
 * @func_type: Function type descriptor
 * @arg_types: Argument types
 * @arg_count: Number of arguments
 *
 * Returns: Return type if valid, NULL if argument mismatch
 *
 * Checks:
 * - Argument count matches parameter count
 * - Each argument type compatible with parameter type
 */
TypeInfo *check_function_call(TypeInfo *func_type,
                              TypeInfo **arg_types, int arg_count);

/**
 * check_array_access - Type check array access
 * @array_type: Array type
 * @index_type: Index type
 *
 * Returns: Element type if valid, NULL otherwise
 *
 * Requires:
 * - array_type must be TYPE_ARRAY
 * - index_type must be TYPE_INT
 */
TypeInfo *check_array_access(TypeInfo *array_type, TypeInfo *index_type);

/* ============================================================================
   Error Reporting Functions
   ============================================================================ */

/**
 * typechecker_error - Report type check error
 * @tc: Type checker context
 * @line, @col: Error location
 * @format: printf-style format string
 * @...: Format arguments
 */
void typechecker_error(TypeChecker *tc, int line, int col,
                      const char *format, ...);

/**
 * typechecker_error_count - Get error count
 *
 * Returns: Number of type check errors collected
 */
int typechecker_error_count(TypeChecker *tc);

/**
 * typechecker_error_get - Get error at index
 * @index: Error index (0-based)
 *
 * Returns: Error pointer, NULL if invalid index
 */
TypeCheckError *typechecker_error_get(TypeChecker *tc, int index);

/**
 * typechecker_print_errors - Print all errors
 */
void typechecker_print_errors(TypeChecker *tc);

/* ============================================================================
   AST Checking Functions
   ============================================================================ */

/**
 * typechecker_check_program - Type check entire program
 * @tc: Type checker context
 * @program: Program AST node
 *
 * Returns: 0 if no errors, number of errors otherwise
 *
 * Performs complete type checking:
 * - Variable declarations
 * - Function definitions
 * - Statements and expressions
 * - Type consistency
 */
int typechecker_check_program(TypeChecker *tc, ASTNode *program);

/**
 * typechecker_check_statement - Type check statement
 * @tc: Type checker context
 * @stmt: Statement node
 *
 * Returns: 0 if valid, -1 if type error
 */
int typechecker_check_statement(TypeChecker *tc, ASTNode *stmt);

/**
 * typechecker_check_expression - Type check expression
 * @tc: Type checker context
 * @expr: Expression node
 *
 * Returns: Inferred type, NULL on error
 */
TypeInfo *typechecker_check_expression(TypeChecker *tc, ASTNode *expr);

/* ============================================================================
   Utility Functions
   ============================================================================ */

/**
 * type_builtin_int - Get built-in int type
 *
 * Returns: Shared int type descriptor
 */
TypeInfo *type_builtin_int(void);

/**
 * type_builtin_float - Get built-in float type
 *
 * Returns: Shared float type descriptor
 */
TypeInfo *type_builtin_float(void);

/**
 * type_builtin_string - Get built-in string type
 *
 * Returns: Shared string type descriptor
 */
TypeInfo *type_builtin_string(void);

/**
 * type_builtin_bool - Get built-in bool type
 *
 * Returns: Shared bool type descriptor
 */
TypeInfo *type_builtin_bool(void);

/**
 * type_builtin_null - Get built-in null type
 *
 * Returns: Shared null type descriptor
 */
TypeInfo *type_builtin_null(void);

/**
 * type_builtin_void - Get built-in void type
 *
 * Returns: Shared void type descriptor
 */
TypeInfo *type_builtin_void(void);

#endif /* FREELANG_TYPECHECKER_H */
