# Error Reporter Guide - Phase 2-C
## Complete Implementation Reference

---

## Table of Contents
1. [Architecture](#architecture)
2. [Core Components](#core-components)
3. [API Reference](#api-reference)
4. [Usage Examples](#usage-examples)
5. [Error Codes](#error-codes)
6. [Testing](#testing)

---

## Architecture

### System Overview

```
┌─────────────────────────────────────────┐
│   Error Reporter System (Phase 2-C)     │
├─────────────────────────────────────────┤
│                                         │
│  fl_error_reporter_t                    │
│  ├─ diagnostics[]                       │
│  ├─ source (source.c, lines[])          │
│  └─ counts (error_count, warning_count) │
│                                         │
│  fl_diagnostic_t                        │
│  ├─ code (E001-E015)                    │
│  ├─ message                             │
│  ├─ location (filename, line, column)   │
│  ├─ annotations[] (underlines)          │
│  └─ hints[] (suggestions)               │
│                                         │
│  fl_annotation_t                        │
│  ├─ col_start, col_end                  │
│  └─ text (explanation)                  │
│                                         │
│  fl_source_t                            │
│  ├─ source (full text)                  │
│  ├─ filename                            │
│  └─ lines[] (fast lookup)               │
│                                         │
└─────────────────────────────────────────┘
```

### Data Flow

```
1. Create Reporter
   fl_error_reporter_new()

2. Set Source Code
   fl_error_reporter_set_source(er, filename, source)
   └─ Splits source into lines for O(1) lookup

3. Add Errors
   fl_error_reporter_add(er, code, message, line, col)
   └─ Returns diagnostic handle

4. Annotate Errors
   fl_error_diagnostic_annotate(d, col_start, col_end, text)
   └─ Adds underline + message

5. Add Hints
   fl_error_diagnostic_hint(d, hint_text)
   └─ Adds suggestion

6. Print Errors
   fl_error_reporter_print_all(er)
   └─ Displays all diagnostics with formatting

7. Cleanup
   fl_error_reporter_free(er)
   └─ Frees all allocations
```

---

## Core Components

### 1. Error Reporter (`fl_error_reporter_t`)

```c
typedef struct {
    fl_diagnostic_t* diagnostics;    /* Array of diagnostics */
    size_t diag_count;               /* Current count */
    size_t diag_capacity;            /* Allocated capacity */
    fl_source_t source;              /* Source code info */
    int error_count;                 /* Error counter */
    int warning_count;               /* Warning counter */
} fl_error_reporter_t;
```

**Purpose**: Collects and manages all diagnostics for a compilation unit

**Lifetime**:
```c
fl_error_reporter_t* er = fl_error_reporter_new();
/* ... add diagnostics ... */
fl_error_reporter_free(er);
```

### 2. Diagnostic (`fl_diagnostic_t`)

```c
typedef struct {
    fl_diag_code_t code;             /* E001-E015 */
    const char* message;             /* Error message */
    const char* filename;            /* Source file */
    int line;                        /* 1-based line number */
    int column;                      /* 0-based column */
    fl_annotation_t* annotations;    /* Underlines */
    size_t annotation_count;         /* Count */
    const char** hints;              /* Suggestions */
    size_t hint_count;               /* Count */
} fl_diagnostic_t;
```

**Purpose**: Represents a single error/warning with location and details

**Lifecycle**:
```c
/* Created by fl_error_reporter_add() */
fl_diagnostic_t* d = fl_error_reporter_add(er, FL_DIAG_E001,
                                            "undefined variable 'x'",
                                            2, 5);

/* Enhanced with annotations and hints */
fl_error_diagnostic_annotate(d, 5, 6, "not found in scope");
fl_error_diagnostic_hint(d, "did you mean 'y'?");

/* Printed via fl_error_reporter_print_all() */
```

### 3. Annotation (`fl_annotation_t`)

```c
typedef struct {
    int col_start;                   /* Column where annotation starts */
    int col_end;                     /* Column where annotation ends (exclusive) */
    const char* text;                /* Message to display */
} fl_annotation_t;
```

**Purpose**: Visual underline with explanation text

**Example**:
```
 2 | let z = x + y;
   |         ^ not found in this scope
   └─ col_start=9, col_end=10, text="not found in this scope"
```

### 4. Source Code (`fl_source_t`)

```c
typedef struct {
    const char* source;              /* Full source code */
    const char* filename;            /* Filename or "<stdin>" */
    fl_source_line_t* lines;         /* Individual lines array */
    size_t line_count;               /* Number of lines */
} fl_source_t;
```

**Purpose**: Efficient source code access

**Optimization**:
- Split source into lines at initialization
- O(1) lookup for any line number
- Pre-allocated array, no substring searching

---

## API Reference

### Error Reporter Management

#### `fl_error_reporter_t* fl_error_reporter_new(void)`

**Purpose**: Create new error reporter

**Returns**: Pointer to initialized reporter or NULL on allocation failure

**Example**:
```c
fl_error_reporter_t* er = fl_error_reporter_new();
if (!er) {
    perror("malloc");
    return -1;
}
```

#### `void fl_error_reporter_free(fl_error_reporter_t* er)`

**Purpose**: Free reporter and all diagnostics

**Preconditions**: Reporter must be created with `fl_error_reporter_new()`

**Side Effects**: Frees all diagnostics, annotations, hints, and source lines

**Example**:
```c
fl_error_reporter_free(er);
er = NULL;  /* Best practice */
```

#### `void fl_error_reporter_set_source(fl_error_reporter_t* er, const char* filename, const char* source)`

**Purpose**: Set source code for error reporting

**Parameters**:
- `er`: Target reporter
- `filename`: Source filename (or "<stdin>" for REPL)
- `source`: Complete source code text

**Side Effects**:
- Frees previous source (if any)
- Splits source into lines
- Creates line lookup table

**Example**:
```c
const char* source = "let x = 1;\nlet y = 2;\n";
fl_error_reporter_set_source(er, "program.fl", source);
```

---

### Diagnostic Management

#### `fl_diagnostic_t* fl_error_reporter_add(fl_error_reporter_t* er, fl_diag_code_t code, const char* message, int line, int column)`

**Purpose**: Add new diagnostic to reporter

**Parameters**:
- `er`: Target reporter
- `code`: Error code (FL_DIAG_E001 through FL_DIAG_E015)
- `message`: Error message (can be NULL, uses default)
- `line`: 1-based line number in source
- `column`: 0-based column number in line

**Returns**: Pointer to created diagnostic for annotation/hints, or NULL on failure

**Increments**: `er->error_count`

**Example**:
```c
fl_diagnostic_t* d = fl_error_reporter_add(er,
    FL_DIAG_E001,
    "undefined variable 'x'",
    3, 10);
if (!d) return NULL;
```

#### `void fl_error_diagnostic_annotate(fl_diagnostic_t* d, int col_start, int col_end, const char* text)`

**Purpose**: Add visual underline to diagnostic

**Parameters**:
- `d`: Target diagnostic
- `col_start`: Starting column (0-based)
- `col_end`: Ending column (exclusive, 0-based)
- `text`: Annotation text

**Side Effects**: Appends annotation to `d->annotations[]`

**Notes**:
- Creates visual underline in error output
- Text appears after underline
- Multiple annotations per diagnostic supported

**Example**:
```c
fl_error_diagnostic_annotate(d, 10, 11, "not found in this scope");
```

Output:
```
let z = x + y;
        ^ not found in this scope
```

#### `void fl_error_diagnostic_hint(fl_diagnostic_t* d, const char* hint)`

**Purpose**: Add hint/suggestion to diagnostic

**Parameters**:
- `d`: Target diagnostic
- `hint`: Suggestion text

**Side Effects**: Appends hint to `d->hints[]`

**Notes**:
- Displayed with "note:" prefix
- Multiple hints per diagnostic supported
- Appears at end of error message

**Example**:
```c
fl_error_diagnostic_hint(d, "did you mean 'y'?");
fl_error_diagnostic_hint(d, "or define 'x' before using it");
```

Output:
```
note: did you mean 'y'?
note: or define 'x' before using it
```

---

### Output Functions

#### `void fl_error_diagnostic_print(fl_diagnostic_t* d, fl_source_t* source)`

**Purpose**: Print single diagnostic with formatting

**Parameters**:
- `d`: Diagnostic to print
- `source`: Source code info (for context lines)

**Output**: Prints to stdout in Rust compiler style

**Format**:
```
ERROR[CODE] message
  --> filename:line:column
   |
 L | source line
 L | error line
   | ^^^^^^^^^ annotation text
 L | next line
   |
note: hint text
```

**Example**:
```c
fl_error_diagnostic_print(d, &er->source);
```

#### `void fl_error_reporter_print_all(fl_error_reporter_t* er)`

**Purpose**: Print all diagnostics with summary

**Parameters**:
- `er`: Reporter containing diagnostics

**Output**: Prints all diagnostics and error summary

**Format**:
```
ERROR[E001] undefined variable 'x'
  --> main.fl:2:10
   | ...
ERROR[E002] undefined function 'foo'
  --> main.fl:3:1
   | ...

error: aborting due to 2 previous errors
```

**Example**:
```c
fl_error_reporter_print_all(er);
```

---

### Query Functions

#### `int fl_error_reporter_error_count(fl_error_reporter_t* er)`

**Purpose**: Get total error count

**Returns**: Number of errors added to reporter

**Example**:
```c
if (fl_error_reporter_error_count(er) > 0) {
    fprintf(stderr, "Compilation failed\n");
    return 1;
}
```

#### `int fl_error_reporter_warning_count(fl_error_reporter_t* er)`

**Purpose**: Get total warning count

**Returns**: Number of warnings (currently not incremented)

**Note**: Reserved for future warning support

#### `const char* fl_error_reporter_get_line(fl_error_reporter_t* er, int line_num)`

**Purpose**: Get source line by line number

**Parameters**:
- `er`: Reporter with source set
- `line_num`: 1-based line number

**Returns**: Line text or NULL if invalid

**Example**:
```c
const char* line = fl_error_reporter_get_line(er, 5);
if (line) printf("Line 5: %s\n", line);
```

---

### Utility Functions

#### `const char* fl_error_diag_code_str(fl_diag_code_t code)`

**Purpose**: Convert error code to string (E001-E015)

**Returns**: String like "E001", "E002", etc.

**Example**:
```c
const char* code_str = fl_error_diag_code_str(FL_DIAG_E001);
/* Returns "E001" */
```

#### `const char* fl_error_diag_default_msg(fl_diag_code_t code)`

**Purpose**: Get default message for error code

**Returns**: Default error message string

**Example**:
```c
const char* msg = fl_error_diag_default_msg(FL_DIAG_E001);
/* Returns "undefined variable" */
```

**Note**: Useful when no custom message is provided

---

## Usage Examples

### Example 1: Basic Error Reporting

```c
#include "error.h"

int main(void) {
    /* Create reporter */
    fl_error_reporter_t* er = fl_error_reporter_new();
    if (!er) return 1;

    /* Set source */
    const char* source = "let x = y + 1;\n";
    fl_error_reporter_set_source(er, "program.fl", source);

    /* Add error */
    fl_diagnostic_t* d = fl_error_reporter_add(er,
        FL_DIAG_E001,
        "undefined variable 'y'",
        1, 8);

    /* Annotate */
    fl_error_diagnostic_annotate(d, 8, 9, "not found in this scope");

    /* Print and exit */
    fl_error_diagnostic_print(d, &er->source);
    fl_error_reporter_free(er);

    return 1;
}
```

Output:
```
ERROR[E001] undefined variable 'y'
  --> program.fl:1:8
   |
 1 | let x = y + 1;
   |         ^ not found in this scope
   |
```

### Example 2: Multiple Errors

```c
fl_error_reporter_t* er = fl_error_reporter_new();
fl_error_reporter_set_source(er, "test.fl",
    "let x = foo(y);\n");

/* Error 1: undefined function */
fl_diagnostic_t* d1 = fl_error_reporter_add(er,
    FL_DIAG_E002, "undefined function 'foo'", 1, 8);
fl_error_diagnostic_annotate(d1, 8, 11, "function not found");

/* Error 2: undefined variable */
fl_diagnostic_t* d2 = fl_error_reporter_add(er,
    FL_DIAG_E001, "undefined variable 'y'", 1, 12);
fl_error_diagnostic_annotate(d2, 12, 13, "not found in this scope");
fl_error_diagnostic_hint(d2, "did you mean 'x'?");

/* Print all */
fl_error_reporter_print_all(er);
fl_error_reporter_free(er);
```

### Example 3: Integration with Parser

```c
/* In parser.c */
fl_error_reporter_t* global_reporter = NULL;

void parser_error(const char* message, fl_token_t* token) {
    if (!global_reporter) return;

    fl_diagnostic_t* d = fl_error_reporter_add(
        global_reporter,
        FL_DIAG_E009,
        message,
        token->line,
        token->column);

    if (d) {
        fl_error_diagnostic_annotate(d,
            token->column,
            token->column + strlen(token->lexeme),
            "syntax error here");
    }
}

int main(void) {
    const char* source = read_file("program.fl");
    global_reporter = fl_error_reporter_new();
    fl_error_reporter_set_source(global_reporter, "program.fl", source);

    /* Parse code - errors reported via parser_error() */
    parse(source);

    /* Print all errors */
    fl_error_reporter_print_all(global_reporter);

    int errors = fl_error_reporter_error_count(global_reporter);
    fl_error_reporter_free(global_reporter);

    return errors > 0 ? 1 : 0;
}
```

---

## Error Codes

### E001: Undefined Variable
```
ERROR[E001] undefined variable 'x'
  --> main.fl:2:10
   |
 2 | let z = x + y;
   |         ^ not found in this scope
   |
note: did you mean 'y'?
```

### E002: Undefined Function
```
ERROR[E002] undefined function 'foo'
  --> main.fl:1:0
   |
 1 | foo();
   | ^^^ function not found
```

### E003: Type Mismatch
```
ERROR[E003] type mismatch
  --> main.fl:1:13
   |
 1 | let x: int = "hello";
   |             ^^^^^^^ expected `int`, found `string`
   |
help: you can convert with `int()`
```

### E004-E015
All error codes implemented with similar format. See `PHASE2C_ERROR_REPORTER.md` for complete list.

---

## Testing

### Running Tests

```bash
# Compile test suite
gcc -Wall -Wextra -O2 -std=c11 -I./include \
    test/test_error.c src/error.c -o bin/test_error -lm

# Run tests
./bin/test_error
```

### Expected Output

```
════════════════════════════════════════════════════════════════
  FreeLang Error Reporter Tests - Phase 2-C
  Rust Compiler Style Error Messages
════════════════════════════════════════════════════════════════

[Test 1] Error reporter creation
─────────────────────────────────────────
✓ PASS

[Test 2] E001 - Undefined variable
─────────────────────────────────────────
ERROR[E001] undefined variable 'x'
...
✓ PASS

... (more tests) ...

════════════════════════════════════════════════════════════════
Test Results:
  Total:  20
  Passed: 20 ✓
  Failed: 0
════════════════════════════════════════════════════════════════
```

---

## Best Practices

1. **Always create reporter**: Use `fl_error_reporter_new()`
2. **Set source before adding errors**: Call `set_source()` early
3. **Use sensible line:column**: Line 1-based, column 0-based
4. **Annotate all errors**: Underlines help users locate problems
5. **Add helpful hints**: Suggestions guide users to fix
6. **Print and clean up**: Always call `fl_error_reporter_free()`
7. **Check error count**: Use `error_count()` before continuing

---

## Performance Characteristics

| Operation | Time | Space |
|-----------|------|-------|
| Create reporter | O(1) | O(1) |
| Set source (n lines) | O(n) | O(n) |
| Add diagnostic | O(1) amortized | O(1) |
| Add annotation | O(1) amortized | O(1) |
| Add hint | O(1) amortized | O(1) |
| Get line | O(1) | - |
| Print diagnostic | O(m) | O(1) |
| Print all (k diagnostics) | O(k*m) | O(1) |
| Free reporter | O(k) | - |

Where: n = source lines, k = diagnostics, m = annotation/hint count

---

## Integration Checklist

- [ ] Include `error.h` in relevant .c files
- [ ] Create global or local `fl_error_reporter_t*`
- [ ] Call `set_source()` with compilation unit
- [ ] Add errors during parsing/semantic analysis
- [ ] Print errors before continuing
- [ ] Return error status to main
- [ ] Free reporter before exit
- [ ] Test with various error combinations

---

**Implementation Date**: 2026-03-06
**Version**: 1.0
**Status**: Production-ready
