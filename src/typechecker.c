/**
 * FreeLang C - Type Checker Implementation
 *
 * Type inference, symbol table management, and semantic analysis
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "ast.h"
#include "typechecker.h"

/* ============================================================================
   HashMap and Vector Interfaces (from myos-lib)
   ============================================================================ */

/* Forward declarations */
typedef struct {
    void *entries;
    size_t capacity;
    size_t count;
    size_t key_size;
    size_t value_size;
    void *hash_func;
    void *cmp_func;
} HashMap;

typedef struct {
    void *data;
    size_t size;
    size_t capacity;
    size_t element_size;
} Vector;

/* HashMap API stubs (would be linked from myos-lib in real build) */
static HashMap *hash_new(size_t key_size, size_t value_size,
                         void *hash_func, void *cmp_func) {
    HashMap *map = (HashMap *)malloc(sizeof(HashMap));
    if (!map) return NULL;

    /* Allocate space for key pointers + value data */
    size_t total_size = 16 * (sizeof(char *) + value_size);
    map->entries = malloc(total_size);
    if (!map->entries) {
        free(map);
        return NULL;
    }

    /* Initialize key pointers to NULL */
    char **key_slots = (char **)map->entries;
    for (int i = 0; i < 16; i++) {
        key_slots[i] = NULL;
    }

    map->capacity = 16;
    map->count = 0;
    map->key_size = key_size;  /* Not used for string keys, but kept for compatibility */
    map->value_size = value_size;
    map->hash_func = hash_func;
    map->cmp_func = cmp_func;

    return map;
}

static void hash_free(HashMap *map) {
    if (!map) return;

    if (map->entries) {
        char **key_slots = (char **)map->entries;
        for (size_t i = 0; i < map->capacity; i++) {
            if (key_slots[i]) free(key_slots[i]);
        }
        free(map->entries);
    }
    free(map);
}

static void hash_set(HashMap *map, const void *key, const void *value) {
    if (!map || !key || !value) return;

    /* Compute hash from string key */
    const char *str_key = (const char *)key;
    size_t h = 5381;
    for (const char *p = str_key; *p; p++) {
        h = ((h << 5) + h) ^ (unsigned char)*p;
    }
    size_t hash = h % map->capacity;
    size_t start = hash;

    /* Store string keys in a simple array (for demo) */
    char **key_slots = (char **)map->entries;
    void **val_slots = (void **)((char *)map->entries + map->capacity * sizeof(char *));

    int found = 0;
    for (size_t i = 0; i < map->capacity; i++) {
        size_t idx = (start + i) % map->capacity;
        if (key_slots[idx] == NULL) {
            key_slots[idx] = (char *)malloc(strlen(str_key) + 1);
            if (key_slots[idx]) {
                strcpy(key_slots[idx], str_key);
                memcpy((char *)val_slots + idx * map->value_size, value, map->value_size);
                map->count++;
            }
            return;
        }
        if (strcmp(key_slots[idx], str_key) == 0) {
            memcpy((char *)val_slots + idx * map->value_size, value, map->value_size);
            return;
        }
    }
}

static void *hash_get(HashMap *map, const void *key) {
    if (!map || !key) return NULL;

    const char *str_key = (const char *)key;
    size_t h = 5381;
    for (const char *p = str_key; *p; p++) {
        h = ((h << 5) + h) ^ (unsigned char)*p;
    }
    size_t hash = h % map->capacity;
    size_t start = hash;

    char **key_slots = (char **)map->entries;
    void **val_slots = (void **)((char *)map->entries + map->capacity * sizeof(char *));

    for (size_t i = 0; i < map->capacity; i++) {
        size_t idx = (start + i) % map->capacity;
        if (key_slots[idx] == NULL) return NULL;

        if (strcmp(key_slots[idx], str_key) == 0) {
            return (char *)val_slots + idx * map->value_size;
        }
    }

    return NULL;
}

/* Vector API stubs */
static Vector *vector_new(size_t element_size) {
    Vector *v = (Vector *)malloc(sizeof(Vector));
    if (!v) return NULL;

    /* Allocate initial capacity with proper alignment */
    size_t initial_capacity = 16;
    v->data = calloc(initial_capacity, element_size);
    if (!v->data) {
        free(v);
        return NULL;
    }

    v->size = 0;
    v->capacity = initial_capacity;
    v->element_size = element_size;

    return v;
}

static void vector_free(Vector *v) {
    if (!v) return;
    if (v->data) free(v->data);
    free(v);
}

static void vector_push(Vector *v, const void *element) {
    if (!v || !element) return;

    if (v->size >= v->capacity) {
        size_t new_capacity = v->capacity * 2;
        void *new_data = realloc(v->data, new_capacity * v->element_size);
        if (!new_data) return;

        v->data = new_data;
        v->capacity = new_capacity;
    }

    if (v->size < v->capacity) {
        char *dest = (char *)v->data + v->size * v->element_size;
        memcpy(dest, element, v->element_size);
        v->size++;
    }
}

static void *vector_get(Vector *v, size_t index) {
    if (!v || index >= v->size) return NULL;
    return (char *)v->data + index * v->element_size;
}

/* ============================================================================
   Built-in Type Singletons
   ============================================================================ */

static TypeInfo *_builtin_int = NULL;
static TypeInfo *_builtin_float = NULL;
static TypeInfo *_builtin_string = NULL;
static TypeInfo *_builtin_bool = NULL;
static TypeInfo *_builtin_null = NULL;
static TypeInfo *_builtin_void = NULL;

static void _init_builtins(void) {
    if (_builtin_int) return;

    _builtin_int = type_new(TYPE_INT, "int");
    _builtin_float = type_new(TYPE_FLOAT, "float");
    _builtin_string = type_new(TYPE_STRING, "string");
    _builtin_bool = type_new(TYPE_BOOL, "bool");
    _builtin_null = type_new(TYPE_NULL, "null");
    _builtin_void = type_new(TYPE_VOID, "void");
}

/* ============================================================================
   Type Creation Functions
   ============================================================================ */

TypeInfo *type_new(TypeKind kind, const char *name) {
    TypeInfo *type = (TypeInfo *)malloc(sizeof(TypeInfo));
    if (!type) return NULL;

    type->kind = kind;
    type->is_nullable = 0;
    type->is_builtin = 1;
    type->element_type = NULL;
    type->return_type = NULL;
    type->param_types = NULL;
    type->param_count = 0;
    type->generic_arg = NULL;
    type->union_types = NULL;
    type->union_count = 0;

    if (name) {
        strncpy(type->name, name, 127);
        type->name[127] = '\0';
    } else {
        type->name[0] = '\0';
    }

    return type;
}

TypeInfo *type_new_array(TypeInfo *element_type) {
    if (!element_type) return NULL;

    TypeInfo *type = type_new(TYPE_ARRAY, "array");
    type->element_type = element_type;
    type->is_builtin = 0;

    return type;
}

TypeInfo *type_new_function(TypeInfo *return_type,
                            TypeInfo **param_types, int param_count) {
    if (!return_type) return NULL;

    TypeInfo *type = type_new(TYPE_FUNCTION, "function");
    type->return_type = return_type;
    type->param_count = param_count;
    type->is_builtin = 0;

    if (param_count > 0 && param_types) {
        type->param_types = (TypeInfo **)malloc(sizeof(TypeInfo *) * param_count);
        if (type->param_types) {
            memcpy(type->param_types, param_types, sizeof(TypeInfo *) * param_count);
        }
    }

    return type;
}

TypeInfo *type_new_generic(const char *name, TypeInfo *arg) {
    TypeInfo *type = type_new(TYPE_GENERIC, name ? name : "T");
    type->generic_arg = arg;
    type->is_builtin = 0;

    return type;
}

TypeInfo *type_new_union(TypeInfo **types, int count) {
    if (!types || count <= 0) return NULL;

    TypeInfo *type = type_new(TYPE_UNION, "union");
    type->union_count = count;
    type->union_types = (TypeInfo **)malloc(sizeof(TypeInfo *) * count);

    if (type->union_types) {
        memcpy(type->union_types, types, sizeof(TypeInfo *) * count);
    }

    type->is_builtin = 0;

    return type;
}

void type_free(TypeInfo *type) {
    if (!type) return;

    /* Don't free built-in singletons */
    if (type == _builtin_int || type == _builtin_float ||
        type == _builtin_string || type == _builtin_bool ||
        type == _builtin_null || type == _builtin_void) {
        return;
    }

    /* Recursively free nested types */
    if (type->element_type) {
        type_free(type->element_type);
    }
    if (type->return_type) {
        type_free(type->return_type);
    }
    if (type->param_types) {
        for (int i = 0; i < type->param_count; i++) {
            type_free(type->param_types[i]);
        }
        free(type->param_types);
    }
    if (type->generic_arg) {
        type_free(type->generic_arg);
    }
    if (type->union_types) {
        for (int i = 0; i < type->union_count; i++) {
            type_free(type->union_types[i]);
        }
        free(type->union_types);
    }

    free(type);
}

/* ============================================================================
   Symbol Functions
   ============================================================================ */

Symbol *symbol_new(const char *name, TypeInfo *type) {
    if (!name || !type) return NULL;

    Symbol *sym = (Symbol *)malloc(sizeof(Symbol));
    if (!sym) return NULL;

    strncpy(sym->name, name, 127);
    sym->name[127] = '\0';

    sym->type = type;
    sym->is_defined = 1;
    sym->is_function = 0;
    sym->is_type = 0;
    sym->is_const = 0;
    sym->line = 0;
    sym->col = 0;
    sym->extra_data = NULL;

    return sym;
}

void symbol_free(Symbol *sym) {
    if (!sym) return;
    if (sym->type) {
        type_free(sym->type);
    }
    free(sym);
}

/* ============================================================================
   Type Checking Functions
   ============================================================================ */

int type_equal(TypeInfo *t1, TypeInfo *t2) {
    if (!t1 || !t2) return 0;

    if (t1->kind != t2->kind) return 0;

    switch (t1->kind) {
        case TYPE_ARRAY:
            return type_equal(t1->element_type, t2->element_type);

        case TYPE_FUNCTION:
            if (t1->param_count != t2->param_count) return 0;
            if (!type_equal(t1->return_type, t2->return_type)) return 0;

            for (int i = 0; i < t1->param_count; i++) {
                if (!type_equal(t1->param_types[i], t2->param_types[i])) {
                    return 0;
                }
            }
            return 1;

        case TYPE_GENERIC:
            return type_equal(t1->generic_arg, t2->generic_arg);

        case TYPE_UNION:
            if (t1->union_count != t2->union_count) return 0;
            for (int i = 0; i < t1->union_count; i++) {
                if (!type_equal(t1->union_types[i], t2->union_types[i])) {
                    return 0;
                }
            }
            return 1;

        default:
            return strcmp(t1->name, t2->name) == 0;
    }
}

int type_compatible(TypeInfo *from, TypeInfo *to) {
    if (!from || !to) return 0;

    /* Same type is always compatible */
    if (type_equal(from, to)) return 1;

    /* int -> float implicit conversion */
    if (from->kind == TYPE_INT && to->kind == TYPE_FLOAT) return 1;

    /* null -> nullable types */
    if (from->kind == TYPE_NULL && to->is_nullable) return 1;

    /* T -> T | U (union membership) */
    if (to->kind == TYPE_UNION) {
        for (int i = 0; i < to->union_count; i++) {
            if (type_equal(from, to->union_types[i])) return 1;
        }
    }

    return 0;
}

char *type_to_string(TypeInfo *type, char *buf, size_t size) {
    if (!type || !buf || size == 0) return buf;

    buf[0] = '\0';

    switch (type->kind) {
        case TYPE_INT:
            snprintf(buf, size, "int");
            break;
        case TYPE_FLOAT:
            snprintf(buf, size, "float");
            break;
        case TYPE_STRING:
            snprintf(buf, size, "string");
            break;
        case TYPE_BOOL:
            snprintf(buf, size, "bool");
            break;
        case TYPE_NULL:
            snprintf(buf, size, "null");
            break;
        case TYPE_VOID:
            snprintf(buf, size, "void");
            break;
        case TYPE_ARRAY: {
            char elem_str[128];
            type_to_string(type->element_type, elem_str, sizeof(elem_str));
            snprintf(buf, size, "array<%s>", elem_str);
            break;
        }
        case TYPE_FUNCTION: {
            char result_str[128] = "(";
            for (int i = 0; i < type->param_count; i++) {
                if (i > 0) strcat(result_str, ", ");
                char param_str[64];
                type_to_string(type->param_types[i], param_str, sizeof(param_str));
                strcat(result_str, param_str);
            }
            strcat(result_str, ") -> ");

            char ret_str[64];
            type_to_string(type->return_type, ret_str, sizeof(ret_str));
            strcat(result_str, ret_str);

            snprintf(buf, size, "%s", result_str);
            break;
        }
        case TYPE_GENERIC: {
            char arg_str[64];
            type_to_string(type->generic_arg, arg_str, sizeof(arg_str));
            snprintf(buf, size, "%s<%s>", type->name, arg_str);
            break;
        }
        case TYPE_UNION: {
            strcpy(buf, "");
            for (int i = 0; i < type->union_count; i++) {
                if (i > 0) strcat(buf, " | ");
                char member_str[64];
                type_to_string(type->union_types[i], member_str, sizeof(member_str));
                strcat(buf, member_str);
            }
            break;
        }
        case TYPE_UNKNOWN:
            snprintf(buf, size, "unknown");
            break;
        default:
            snprintf(buf, size, "%s", type->name[0] ? type->name : "?");
    }

    return buf;
}

TypeInfo *check_binary_op(TypeInfo *left, const char *op, TypeInfo *right) {
    if (!left || !right || !op) return NULL;

    _init_builtins();

    /* Arithmetic: +, -, *, /, % */
    if (strcmp(op, "+") == 0) {
        if (left->kind == TYPE_INT && right->kind == TYPE_INT) return _builtin_int;
        if (left->kind == TYPE_INT && right->kind == TYPE_FLOAT) return _builtin_float;
        if (left->kind == TYPE_FLOAT && right->kind == TYPE_INT) return _builtin_float;
        if (left->kind == TYPE_FLOAT && right->kind == TYPE_FLOAT) return _builtin_float;
        if (left->kind == TYPE_STRING && right->kind == TYPE_STRING) return _builtin_string;
        return NULL;
    }

    if (strcmp(op, "-") == 0 || strcmp(op, "*") == 0 || strcmp(op, "/") == 0) {
        if (left->kind == TYPE_INT && right->kind == TYPE_INT) return _builtin_int;
        if (left->kind == TYPE_INT && right->kind == TYPE_FLOAT) return _builtin_float;
        if (left->kind == TYPE_FLOAT && right->kind == TYPE_INT) return _builtin_float;
        if (left->kind == TYPE_FLOAT && right->kind == TYPE_FLOAT) return _builtin_float;
        return NULL;
    }

    if (strcmp(op, "%") == 0) {
        if (left->kind == TYPE_INT && right->kind == TYPE_INT) return _builtin_int;
        return NULL;
    }

    /* Comparison: ==, !=, <, >, <=, >= */
    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 ||
        strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
        strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
        if ((left->kind == TYPE_INT || left->kind == TYPE_FLOAT) &&
            (right->kind == TYPE_INT || right->kind == TYPE_FLOAT)) {
            return _builtin_bool;
        }
        if (left->kind == TYPE_STRING && right->kind == TYPE_STRING) {
            return _builtin_bool;
        }
        if (left->kind == TYPE_BOOL && right->kind == TYPE_BOOL) {
            return _builtin_bool;
        }
        return NULL;
    }

    /* Logical: &&, || */
    if (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
        if (left->kind == TYPE_BOOL && right->kind == TYPE_BOOL) {
            return _builtin_bool;
        }
        return NULL;
    }

    return NULL;
}

TypeInfo *check_unary_op(const char *op, TypeInfo *operand) {
    if (!op || !operand) return NULL;

    _init_builtins();

    if (strcmp(op, "-") == 0) {
        if (operand->kind == TYPE_INT) return _builtin_int;
        if (operand->kind == TYPE_FLOAT) return _builtin_float;
        return NULL;
    }

    if (strcmp(op, "!") == 0) {
        if (operand->kind == TYPE_BOOL) return _builtin_bool;
        return NULL;
    }

    return NULL;
}

TypeInfo *check_function_call(TypeInfo *func_type,
                              TypeInfo **arg_types, int arg_count) {
    if (!func_type || func_type->kind != TYPE_FUNCTION) return NULL;

    if (arg_count != func_type->param_count) return NULL;

    for (int i = 0; i < arg_count; i++) {
        if (!type_compatible(arg_types[i], func_type->param_types[i])) {
            return NULL;
        }
    }

    return func_type->return_type;
}

TypeInfo *check_array_access(TypeInfo *array_type, TypeInfo *index_type) {
    if (!array_type || !index_type) return NULL;

    _init_builtins();

    if (array_type->kind != TYPE_ARRAY) return NULL;
    if (index_type->kind != TYPE_INT) return NULL;

    return array_type->element_type;
}

/* ============================================================================
   Type Inference
   ============================================================================ */

TypeInfo *infer_type(TypeChecker *tc, ASTNode *node) {
    if (!tc || !node) return NULL;

    _init_builtins();

    /* Literal inference */
    if (node->type == NODE_LITERAL) {
        /* node->data.literal would contain type info */
        /* For now, return unknown */
        return type_new(TYPE_UNKNOWN, "?");
    }

    /* Identifier inference (variable lookup) */
    if (node->type == NODE_IDENT) {
        /* node->data.ident.name */
        Symbol *sym = typechecker_lookup(tc, "unknown");  /* Would use node->data.ident.name */
        if (sym) return sym->type;
        return NULL;
    }

    return type_new(TYPE_UNKNOWN, "?");
}

/* ============================================================================
   TypeChecker Constructor/Destructor
   ============================================================================ */

TypeChecker *typechecker_new(void) {
    _init_builtins();

    TypeChecker *tc = (TypeChecker *)malloc(sizeof(TypeChecker));
    if (!tc) return NULL;

    /* Create global scope */
    SymbolTable *global_scope = (SymbolTable *)malloc(sizeof(SymbolTable));
    if (!global_scope) {
        free(tc);
        return NULL;
    }

    global_scope->symbols = hash_new(128, sizeof(Symbol *), NULL, NULL);
    global_scope->parent = NULL;
    global_scope->depth = 0;

    /* Create scope container */
    Scope *scope = (Scope *)malloc(sizeof(Scope));
    if (!scope) {
        hash_free((HashMap *)global_scope->symbols);
        free(global_scope);
        free(tc);
        return NULL;
    }

    scope->current = global_scope;
    scope->scope_stack = vector_new(sizeof(SymbolTable *));

    if (!scope->scope_stack) {
        hash_free((HashMap *)global_scope->symbols);
        free(global_scope);
        free(scope);
        free(tc);
        return NULL;
    }

    /* Push global scope onto stack */
    vector_push((Vector *)scope->scope_stack, &global_scope);

    /* Initialize type checker */
    tc->scope = scope;
    tc->error_list = vector_new(sizeof(TypeCheckError));
    tc->error_count = 0;
    tc->current_fn_return = NULL;

    return tc;
}

void typechecker_free(TypeChecker *tc) {
    if (!tc) return;

    if (tc->scope) {
        if (tc->scope->scope_stack) {
            vector_free((Vector *)tc->scope->scope_stack);
        }

        SymbolTable *current = tc->scope->current;
        while (current) {
            SymbolTable *parent = current->parent;
            if (current->symbols) {
                hash_free((HashMap *)current->symbols);
            }
            free(current);
            current = parent;
        }

        free(tc->scope);
    }

    if (tc->error_list) {
        vector_free((Vector *)tc->error_list);
    }

    free(tc);
}

/* ============================================================================
   Scope Management
   ============================================================================ */

void typechecker_push_scope(TypeChecker *tc) {
    if (!tc || !tc->scope) return;

    SymbolTable *new_scope = (SymbolTable *)malloc(sizeof(SymbolTable));
    if (!new_scope) return;

    new_scope->symbols = hash_new(128, sizeof(Symbol *), NULL, NULL);
    new_scope->parent = tc->scope->current;
    new_scope->depth = tc->scope->current ? tc->scope->current->depth + 1 : 0;

    if (new_scope->symbols) {
        tc->scope->current = new_scope;
        vector_push((Vector *)tc->scope->scope_stack, &new_scope);
    } else {
        free(new_scope);
    }
}

int typechecker_pop_scope(TypeChecker *tc) {
    if (!tc || !tc->scope || !tc->scope->current) return -1;

    if (!tc->scope->current->parent) return -1;  /* Can't pop global scope */

    SymbolTable *popped = tc->scope->current;
    tc->scope->current = popped->parent;

    if (popped->symbols) {
        hash_free((HashMap *)popped->symbols);
    }
    free(popped);

    return 0;
}

int typechecker_scope_depth(TypeChecker *tc) {
    if (!tc || !tc->scope || !tc->scope->current) return -1;
    return tc->scope->current->depth;
}

/* ============================================================================
   Symbol Definition & Lookup
   ============================================================================ */

int typechecker_define(TypeChecker *tc, const char *name, TypeInfo *type) {
    if (!tc || !tc->scope || !tc->scope->current || !name || !type) return -1;

    /* Check if already defined in current scope */
    Symbol *existing = (Symbol *)hash_get((HashMap *)tc->scope->current->symbols, name);
    if (existing) return -1;

    /* Create and insert new symbol */
    Symbol *sym = symbol_new(name, type);
    if (!sym) return -1;

    hash_set((HashMap *)tc->scope->current->symbols, name, &sym);

    return 0;
}

int typechecker_define_function(TypeChecker *tc, const char *name,
                                TypeInfo *return_type,
                                TypeInfo **param_types, int param_count) {
    if (!tc || !name || !return_type) return -1;

    TypeInfo *func_type = type_new_function(return_type, param_types, param_count);
    if (!func_type) return -1;

    int result = typechecker_define(tc, name, func_type);

    if (result == 0) {
        Symbol *sym = (Symbol *)hash_get((HashMap *)tc->scope->current->symbols, name);
        if (sym) {
            sym->is_function = 1;
        }
    } else {
        type_free(func_type);
    }

    return result;
}

int typechecker_define_type(TypeChecker *tc, const char *name) {
    if (!tc || !name) return -1;

    TypeInfo *type = type_new(TYPE_UNKNOWN, name);
    if (!type) return -1;

    int result = typechecker_define(tc, name, type);

    if (result == 0) {
        Symbol *sym = (Symbol *)hash_get((HashMap *)tc->scope->current->symbols, name);
        if (sym) {
            sym->is_type = 1;
        }
    } else {
        type_free(type);
    }

    return result;
}

Symbol *typechecker_lookup(TypeChecker *tc, const char *name) {
    if (!tc || !name) return NULL;

    SymbolTable *current = tc->scope->current;

    while (current) {
        Symbol *sym = (Symbol *)hash_get((HashMap *)current->symbols, name);
        if (sym) return sym;
        current = current->parent;
    }

    return NULL;
}

Symbol *typechecker_lookup_current(TypeChecker *tc, const char *name) {
    if (!tc || !tc->scope || !tc->scope->current || !name) return NULL;

    return (Symbol *)hash_get((HashMap *)tc->scope->current->symbols, name);
}

int typechecker_is_defined(TypeChecker *tc, const char *name) {
    return typechecker_lookup(tc, name) != NULL;
}

/* ============================================================================
   Error Reporting
   ============================================================================ */

void typechecker_error(TypeChecker *tc, int line, int col,
                      const char *format, ...) {
    if (!tc || !format) return;

    TypeCheckError err;
    err.line = line;
    err.col = col;

    va_list args;
    va_start(args, format);
    vsnprintf(err.message, sizeof(err.message), format, args);
    va_end(args);

    vector_push((Vector *)tc->error_list, &err);
    tc->error_count++;
}

int typechecker_error_count(TypeChecker *tc) {
    if (!tc) return 0;
    return tc->error_count;
}

TypeCheckError *typechecker_error_get(TypeChecker *tc, int index) {
    if (!tc || index < 0 || index >= tc->error_count) return NULL;
    return (TypeCheckError *)vector_get((Vector *)tc->error_list, index);
}

void typechecker_print_errors(TypeChecker *tc) {
    if (!tc) return;

    for (int i = 0; i < tc->error_count; i++) {
        TypeCheckError *err = typechecker_error_get(tc, i);
        if (err) {
            printf("Line %d, Col %d: %s\n", err->line, err->col, err->message);
        }
    }
}

/* ============================================================================
   AST Checking (Stub implementations)
   ============================================================================ */

int typechecker_check_program(TypeChecker *tc, ASTNode *program) {
    if (!tc || !program) return -1;

    /* TODO: Implement full program checking */
    /* - Iterate through declarations and statements
     * - Check each variable declaration, function definition
     * - Check statement types
     * - Return error count
     */

    return tc->error_count;
}

int typechecker_check_statement(TypeChecker *tc, ASTNode *stmt) {
    if (!tc || !stmt) return -1;

    /* TODO: Implement statement checking */

    return 0;
}

TypeInfo *typechecker_check_expression(TypeChecker *tc, ASTNode *expr) {
    if (!tc || !expr) return NULL;

    /* TODO: Implement expression checking */

    return type_new(TYPE_UNKNOWN, "?");
}

/* ============================================================================
   Built-in Type Accessors
   ============================================================================ */

TypeInfo *type_builtin_int(void) {
    _init_builtins();
    return _builtin_int;
}

TypeInfo *type_builtin_float(void) {
    _init_builtins();
    return _builtin_float;
}

TypeInfo *type_builtin_string(void) {
    _init_builtins();
    return _builtin_string;
}

TypeInfo *type_builtin_bool(void) {
    _init_builtins();
    return _builtin_bool;
}

TypeInfo *type_builtin_null(void) {
    _init_builtins();
    return _builtin_null;
}

TypeInfo *type_builtin_void(void) {
    _init_builtins();
    return _builtin_void;
}
