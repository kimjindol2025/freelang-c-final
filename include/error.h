/**
 * FreeLang Error Handling & Diagnostic Reporting
 * Exception handling and advanced error reporting (Rust compiler style)
 */

#ifndef FL_ERROR_H
#define FL_ERROR_H

#include <setjmp.h>
#include <stddef.h>
#include "freelang.h"

/* =================================================================== */
/* Phase 2-C: Advanced Error Reporter (Rust compiler style)            */
/* =================================================================== */

/* Diagnostic error codes (E001-E015) */
typedef enum {
    FL_DIAG_E001,  /* Undefined variable */
    FL_DIAG_E002,  /* Undefined function */
    FL_DIAG_E003,  /* Type mismatch */
    FL_DIAG_E004,  /* Invalid operator */
    FL_DIAG_E005,  /* Invalid argument count */
    FL_DIAG_E006,  /* Duplicate definition */
    FL_DIAG_E007,  /* Invalid index */
    FL_DIAG_E008,  /* Invalid member access */
    FL_DIAG_E009,  /* Syntax error */
    FL_DIAG_E010,  /* Unexpected token */
    FL_DIAG_E011,  /* Unclosed bracket */
    FL_DIAG_E012,  /* Invalid escape sequence */
    FL_DIAG_E013,  /* Divide by zero */
    FL_DIAG_E014,  /* Unreachable code */
    FL_DIAG_E015   /* Missing return */
} fl_diag_code_t;

/* Source code line information */
typedef struct {
    const char* line_text;
    int line_num;
} fl_source_line_t;

/* Source code context */
typedef struct {
    const char* source;         /* Full source code */
    const char* filename;       /* Filename or "<stdin>" */
    fl_source_line_t* lines;    /* Array of lines for fast lookup */
    size_t line_count;          /* Number of lines */
} fl_source_t;

/* Annotation for diagnostic (underline + message) */
typedef struct {
    int col_start;              /* Column start (0-based) */
    int col_end;                /* Column end (exclusive) */
    const char* text;           /* Annotation text (e.g., "not found in this scope") */
} fl_annotation_t;

/* Single diagnostic (error/warning) */
typedef struct {
    fl_diag_code_t code;        /* E001-E015 */
    const char* message;        /* Main error message */
    const char* filename;
    int line;                   /* 1-based line number */
    int column;                 /* 0-based column number */
    fl_annotation_t* annotations;     /* Underline + messages */
    size_t annotation_count;
    const char** hints;         /* Hint/suggestion messages */
    size_t hint_count;
} fl_diagnostic_t;

/* Error reporter (collects all diagnostics) */
typedef struct {
    fl_diagnostic_t* diagnostics;    /* Array of diagnostics */
    size_t diag_count;
    size_t diag_capacity;
    fl_source_t source;              /* Source info */
    int error_count;
    int warning_count;
} fl_error_reporter_t;

/* Error reporter creation/destruction */
fl_error_reporter_t* fl_error_reporter_new(void);
void fl_error_reporter_free(fl_error_reporter_t* er);

/* Set source code for reporter */
void fl_error_reporter_set_source(fl_error_reporter_t* er,
                                   const char* filename,
                                   const char* source);

/* Add diagnostic */
fl_diagnostic_t* fl_error_reporter_add(fl_error_reporter_t* er,
                                        fl_diag_code_t code,
                                        const char* message,
                                        int line, int column);

/* Add annotation (underline) to diagnostic */
void fl_error_diagnostic_annotate(fl_diagnostic_t* d,
                                   int col_start, int col_end,
                                   const char* text);

/* Add hint to diagnostic */
void fl_error_diagnostic_hint(fl_diagnostic_t* d, const char* hint);

/* Print single diagnostic */
void fl_error_diagnostic_print(fl_diagnostic_t* d, fl_source_t* source);

/* Print all diagnostics */
void fl_error_reporter_print_all(fl_error_reporter_t* er);

/* Get error count */
int fl_error_reporter_error_count(fl_error_reporter_t* er);
int fl_error_reporter_warning_count(fl_error_reporter_t* er);

/* Get source line (1-based line number) */
const char* fl_error_reporter_get_line(fl_error_reporter_t* er, int line_num);

/* Get diagnostic code as string */
const char* fl_error_diag_code_str(fl_diag_code_t code);

/* Get default error message for code */
const char* fl_error_diag_default_msg(fl_diag_code_t code);

/* =================================================================== */
/* Legacy error handling (for compatibility)                           */
/* =================================================================== */

/* Error creation (uses fl_error_t from freelang.h) */
fl_error_t* fl_error_create(fl_error_type_t type, const char* message,
                           const char* filename, int line, int column);

/* Error destruction */
void fl_error_destroy(fl_error_t* error);

/* Error formatting */
char* fl_error_format(fl_error_t* error);
void fl_error_print(fl_error_t* error);

/* Stack trace */
void fl_error_append_stack(fl_error_t* error, const char* function, int line);
void fl_error_print_stack_trace(fl_error_t* error);

/* Convert error to string */
const char* fl_error_type_name(fl_error_type_t type);

/* Global error context */
typedef struct {
    fl_error_t* current_error;
    bool has_error;
    jmp_buf error_handler;
} fl_error_context_t;

/* Get/set global error context */
fl_error_context_t* fl_error_get_context(void);
void fl_error_set_context(fl_error_context_t* ctx);

/* Throw error (uses setjmp/longjmp) */
#define FL_THROW_ERROR(type, msg) \
    do { \
        fl_error_context_t* ctx = fl_error_get_context(); \
        ctx->current_error = fl_error_create(type, msg, __FILE__, __LINE__, 0); \
        ctx->has_error = true; \
        longjmp(ctx->error_handler, 1); \
    } while(0)

#endif /* FL_ERROR_H */
