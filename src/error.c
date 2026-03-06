/**
 * FreeLang Error Handling Implementation
 * Phase 2-C: Advanced Error Reporter (Rust compiler style)
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "../include/error.h"

/* =================================================================== */
/* Phase 2-C: Error Reporter Implementation                            */
/* =================================================================== */

#define ANSI_RED     "\x1b[31m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_CYAN    "\x1b[36m"
#define ANSI_BOLD    "\x1b[1m"
#define ANSI_RESET   "\x1b[0m"

/* Forward declarations */
static fl_source_line_t* _split_source_into_lines(const char* source, size_t* out_count);
static void _free_source_lines(fl_source_line_t* lines, size_t count);

/**
 * Create new error reporter
 */
fl_error_reporter_t* fl_error_reporter_new(void) {
    fl_error_reporter_t* er = (fl_error_reporter_t*)malloc(sizeof(fl_error_reporter_t));
    if (!er) return NULL;

    er->diagnostics = NULL;
    er->diag_count = 0;
    er->diag_capacity = 0;
    er->source.source = NULL;
    er->source.filename = NULL;
    er->source.lines = NULL;
    er->source.line_count = 0;
    er->error_count = 0;
    er->warning_count = 0;

    return er;
}

/**
 * Free error reporter and all diagnostics
 */
void fl_error_reporter_free(fl_error_reporter_t* er) {
    if (!er) return;

    if (er->diagnostics) {
        for (size_t i = 0; i < er->diag_count; i++) {
            fl_diagnostic_t* d = &er->diagnostics[i];
            if (d->annotations) free(d->annotations);
            if (d->hints) {
                for (size_t j = 0; j < d->hint_count; j++) {
                    free((void*)d->hints[j]);
                }
                free(d->hints);
            }
        }
        free(er->diagnostics);
    }

    if (er->source.lines) {
        _free_source_lines(er->source.lines, er->source.line_count);
    }

    if (er->source.filename) free((void*)er->source.filename);
    free(er);
}

/**
 * Split source code into lines for fast lookup
 */
static fl_source_line_t* _split_source_into_lines(const char* source, size_t* out_count) {
    if (!source) {
        *out_count = 0;
        return NULL;
    }

    /* Count lines */
    size_t line_count = 1;
    for (const char* p = source; *p; p++) {
        if (*p == '\n') line_count++;
    }

    fl_source_line_t* lines = (fl_source_line_t*)malloc(sizeof(fl_source_line_t) * line_count);
    if (!lines) {
        *out_count = 0;
        return NULL;
    }

    /* Extract lines */
    size_t line_idx = 0;
    const char* line_start = source;
    const char* p = source;

    while (*p && line_idx < line_count) {
        if (*p == '\n') {
            size_t line_len = p - line_start;
            char* line_copy = (char*)malloc(line_len + 1);
            if (line_copy) {
                strncpy(line_copy, line_start, line_len);
                line_copy[line_len] = '\0';
            }
            lines[line_idx].line_text = line_copy;
            lines[line_idx].line_num = line_idx + 1;
            line_idx++;
            line_start = p + 1;
        }
        p++;
    }

    /* Last line */
    if (line_idx < line_count && line_start < p) {
        size_t line_len = p - line_start;
        char* line_copy = (char*)malloc(line_len + 1);
        if (line_copy) {
            strncpy(line_copy, line_start, line_len);
            line_copy[line_len] = '\0';
        }
        lines[line_idx].line_text = line_copy;
        lines[line_idx].line_num = line_idx + 1;
    }

    *out_count = line_count;
    return lines;
}

/**
 * Free source lines
 */
static void _free_source_lines(fl_source_line_t* lines, size_t count) {
    if (!lines) return;
    for (size_t i = 0; i < count; i++) {
        if (lines[i].line_text) free((void*)lines[i].line_text);
    }
    free(lines);
}

/**
 * Set source code for reporter
 */
void fl_error_reporter_set_source(fl_error_reporter_t* er,
                                   const char* filename,
                                   const char* source) {
    if (!er) return;

    /* Free old source info */
    if (er->source.filename) free((void*)er->source.filename);
    if (er->source.lines) _free_source_lines(er->source.lines, er->source.line_count);

    /* Set new source */
    er->source.source = source;
    er->source.filename = filename ? strdup(filename) : strdup("<stdin>");
    er->source.lines = _split_source_into_lines(source, &er->source.line_count);
}

/**
 * Add diagnostic to reporter
 */
fl_diagnostic_t* fl_error_reporter_add(fl_error_reporter_t* er,
                                        fl_diag_code_t code,
                                        const char* message,
                                        int line, int column) {
    if (!er) return NULL;

    /* Expand capacity if needed */
    if (er->diag_count >= er->diag_capacity) {
        er->diag_capacity = (er->diag_capacity == 0) ? 16 : er->diag_capacity * 2;
        fl_diagnostic_t* new_diags = (fl_diagnostic_t*)realloc(er->diagnostics,
                                                                sizeof(fl_diagnostic_t) * er->diag_capacity);
        if (!new_diags) return NULL;
        er->diagnostics = new_diags;
    }

    fl_diagnostic_t* d = &er->diagnostics[er->diag_count];
    er->diag_count++;

    d->code = code;
    d->message = message;
    d->filename = er->source.filename;
    d->line = line;
    d->column = column;
    d->annotations = NULL;
    d->annotation_count = 0;
    d->hints = NULL;
    d->hint_count = 0;

    er->error_count++;

    return d;
}

/**
 * Add annotation (underline) to diagnostic
 */
void fl_error_diagnostic_annotate(fl_diagnostic_t* d,
                                   int col_start, int col_end,
                                   const char* text) {
    if (!d) return;

    fl_annotation_t* new_annot = (fl_annotation_t*)realloc(d->annotations,
                                                            sizeof(fl_annotation_t) * (d->annotation_count + 1));
    if (!new_annot) return;

    d->annotations = new_annot;
    d->annotations[d->annotation_count].col_start = col_start;
    d->annotations[d->annotation_count].col_end = col_end;
    d->annotations[d->annotation_count].text = text;
    d->annotation_count++;
}

/**
 * Add hint to diagnostic
 */
void fl_error_diagnostic_hint(fl_diagnostic_t* d, const char* hint) {
    if (!d) return;

    const char** new_hints = (const char**)realloc(d->hints, sizeof(const char*) * (d->hint_count + 1));
    if (!new_hints) return;

    d->hints = new_hints;
    d->hints[d->hint_count] = strdup(hint);
    d->hint_count++;
}

/**
 * Print single diagnostic with Rust-style formatting
 */
void fl_error_diagnostic_print(fl_diagnostic_t* d, fl_source_t* source) {
    if (!d) return;

    /* Header: ERROR[CODE] message */
    printf(ANSI_RED ANSI_BOLD "ERROR" ANSI_RESET "[%s] %s\n",
           fl_error_diag_code_str(d->code), d->message);

    /* Location: --> filename:line:column */
    printf("  " ANSI_CYAN "-->" ANSI_RESET " %s:%d:%d\n",
           d->filename, d->line, d->column);

    if (!source || !source->lines) return;

    /* Print context lines */
    printf("   |\n");

    int line_idx = d->line - 1;
    if (line_idx >= 0 && line_idx < (int)source->line_count) {
        /* Line before (if exists) */
        if (line_idx > 0) {
            printf(" %d | %s\n", line_idx, source->lines[line_idx - 1].line_text);
        }

        /* Error line */
        printf(" %d | %s\n", d->line, source->lines[line_idx].line_text);

        /* Annotation underline */
        printf("   | ");
        if (d->annotation_count > 0) {
            int col = 0;
            fl_annotation_t* annot = &d->annotations[0];

            /* Print spaces before annotation */
            while (col < annot->col_start) {
                printf(" ");
                col++;
            }

            /* Print caret underline */
            printf(ANSI_RED);
            while (col < annot->col_end) {
                printf("^");
                col++;
            }
            printf(ANSI_RESET " %s\n", annot->text);
        }

        /* Line after (if exists) */
        if (line_idx + 1 < (int)source->line_count) {
            printf(" %d | %s\n", line_idx + 2, source->lines[line_idx + 1].line_text);
        }
    }

    printf("   |\n");

    /* Print hints */
    if (d->hint_count > 0) {
        for (size_t i = 0; i < d->hint_count; i++) {
            printf(ANSI_YELLOW "note:" ANSI_RESET " %s\n", d->hints[i]);
        }
    }

    printf("\n");
}

/**
 * Print all diagnostics
 */
void fl_error_reporter_print_all(fl_error_reporter_t* er) {
    if (!er) return;

    for (size_t i = 0; i < er->diag_count; i++) {
        fl_error_diagnostic_print(&er->diagnostics[i], &er->source);
    }

    /* Summary */
    if (er->error_count > 0) {
        printf(ANSI_RED ANSI_BOLD "error" ANSI_RESET ": ");
        if (er->error_count == 1) {
            printf("aborting due to previous error\n");
        } else {
            printf("aborting due to %d previous errors\n", er->error_count);
        }
    }
}

/**
 * Get error count
 */
int fl_error_reporter_error_count(fl_error_reporter_t* er) {
    return er ? er->error_count : 0;
}

/**
 * Get warning count
 */
int fl_error_reporter_warning_count(fl_error_reporter_t* er) {
    return er ? er->warning_count : 0;
}

/**
 * Get source line by line number (1-based)
 */
const char* fl_error_reporter_get_line(fl_error_reporter_t* er, int line_num) {
    if (!er || !er->source.lines || line_num < 1 || line_num > (int)er->source.line_count) {
        return NULL;
    }
    return er->source.lines[line_num - 1].line_text;
}

/**
 * Get diagnostic code as string
 */
const char* fl_error_diag_code_str(fl_diag_code_t code) {
    static char buffer[16];
    snprintf(buffer, sizeof(buffer), "E%03d", (int)code + 1);
    return buffer;
}

/**
 * Get default error message for code
 */
const char* fl_error_diag_default_msg(fl_diag_code_t code) {
    switch (code) {
        case FL_DIAG_E001: return "undefined variable";
        case FL_DIAG_E002: return "undefined function";
        case FL_DIAG_E003: return "type mismatch";
        case FL_DIAG_E004: return "invalid operator";
        case FL_DIAG_E005: return "invalid argument count";
        case FL_DIAG_E006: return "duplicate definition";
        case FL_DIAG_E007: return "invalid index";
        case FL_DIAG_E008: return "invalid member access";
        case FL_DIAG_E009: return "syntax error";
        case FL_DIAG_E010: return "unexpected token";
        case FL_DIAG_E011: return "unclosed bracket";
        case FL_DIAG_E012: return "invalid escape sequence";
        case FL_DIAG_E013: return "divide by zero";
        case FL_DIAG_E014: return "unreachable code";
        case FL_DIAG_E015: return "missing return";
        default:           return "unknown error";
    }
}

/* =================================================================== */
/* Legacy error handling (for compatibility)                           */
/* =================================================================== */

fl_error_t* fl_error_create(fl_error_type_t type, const char* message,
                           const char* filename, int line, int column) {
    fl_error_t* error = (fl_error_t*)malloc(sizeof(fl_error_t));
    if (!error) return NULL;

    error->type = type;
    error->message = (char*)malloc(strlen(message) + 1);
    if (!error->message) {
        free(error);
        return NULL;
    }
    strcpy(error->message, message);

    error->filename = (char*)malloc(strlen(filename) + 1);
    if (!error->filename) {
        free(error->message);
        free(error);
        return NULL;
    }
    strcpy(error->filename, filename);

    error->line = line;
    error->column = column;
    error->stack_trace = NULL;

    return error;
}

void fl_error_destroy(fl_error_t* error) {
    if (error) {
        if (error->message) free(error->message);
        if (error->filename) free(error->filename);
        if (error->stack_trace) free(error->stack_trace);
        free(error);
    }
}

char* fl_error_format(fl_error_t* error) {
    char* buffer = (char*)malloc(1024);
    if (!buffer) return NULL;

    snprintf(buffer, 1024, "%s at %s:%d:%d: %s",
             fl_error_type_name(error->type),
             error->filename,
             error->line,
             error->column,
             error->message);

    return buffer;
}

void fl_error_print(fl_error_t* error) {
    char* formatted = fl_error_format(error);
    if (formatted) {
        fprintf(stderr, "%s\n", formatted);
        free(formatted);
    }
}

void fl_error_append_stack(fl_error_t* error, const char* function, int line) {
    /* TODO: Implement stack trace appending */
}

void fl_error_print_stack_trace(fl_error_t* error) {
    if (error->stack_trace) {
        fprintf(stderr, "%s\n", error->stack_trace);
    }
}

const char* fl_error_type_name(fl_error_type_t type) {
    switch (type) {
        case FL_ERR_NONE:                    return "None";
        case FL_ERR_SYNTAX:                  return "SyntaxError";
        case FL_ERR_RUNTIME:                 return "RuntimeError";
        case FL_ERR_TYPE:                    return "TypeError";
        case FL_ERR_REFERENCE:               return "ReferenceError";
        case FL_ERR_INDEX_OUT_OF_BOUNDS:     return "IndexOutOfBoundsError";
        case FL_ERR_DIVIDE_BY_ZERO:          return "DivideByZeroError";
        case FL_ERR_UNDEFINED_VARIABLE:      return "UndefinedVariableError";
        case FL_ERR_UNDEFINED_FUNCTION:      return "UndefinedFunctionError";
        case FL_ERR_INVALID_ARGUMENT:        return "InvalidArgumentError";
        case FL_ERR_ASSERTION_FAILED:        return "AssertionError";
        case FL_ERR_IO:                      return "IOError";
        case FL_ERR_CUSTOM:                  return "CustomError";
        default:                             return "UnknownError";
    }
}
