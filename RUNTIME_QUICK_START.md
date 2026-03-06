# FreeLang C Runtime - Quick Start Guide

## Build

```bash
cd /home/kimjin/Desktop/kim/freelang-c
make clean all
```

**Result**: `bin/fl` executable

---

## Usage

### 1. Execute a File
```bash
./bin/fl run program.fl
```

**Example**:
```bash
# Create a test file
cat > test.fl << 'EOF'
let x = 42;
let y = 100;
let z = x + y;
EOF

# Run it
./bin/fl run test.fl
```

### 2. Interactive REPL
```bash
./bin/fl repl
```

**Interactive Example**:
```
FreeLang v1.0.0 REPL
Type 'exit' or 'quit' to quit

fl> let x = 10;
fl> let y = 20;
fl> let sum = x + y;
fl> exit
Goodbye!
```

### 3. Run Tests
```bash
./bin/fl test
```

---

## Pipeline Architecture

```
.fl source file
    ↓
[LEXER]      → Tokenization
    ↓
[PARSER]     → AST generation
    ↓
[COMPILER]   → Bytecode compilation
    ↓
[VM]         → Bytecode execution
    ↓
Result/Output
```

---

## Global Variables

FreeLang supports global variable declarations:

```freelang
let x = 10;           // Initialize global x
let y = x + 5;        // Use x in expression
```

Variables are stored in the runtime's global store and persist across statements.

---

## Error Handling

Errors are reported with context:

```
Error [type]: message
  File: program.fl
  Line: 5, Col: 10
```

Error types:
- `FL_ERR_SYNTAX` - Parse error
- `FL_ERR_RUNTIME` - Runtime error
- `FL_ERR_TYPE` - Type mismatch
- `FL_ERR_IO` - File I/O error

---

## Supported Features (Phase 3-C)

✅ **Implemented**:
- Variable declarations (`let`, `const`, `var`)
- Integer/float literals
- String literals
- Boolean literals
- Array literals
- Basic expressions (operators TBD)
- If/else statements (structure in place)
- While loops (structure in place)
- For loops (structure in place)
- Try-catch-finally (structure in place)
- Global variable storage

⏳ **In Progress** (Phase 3-D):
- Builtin functions (`println`, `str`, etc.)
- Arithmetic operators
- Comparison operators
- Function definitions and calls
- Proper error messages

---

## Key Functions (C API)

### Create Runtime
```c
fl_runtime_t *runtime = fl_runtime_create();
```

### Evaluate Source Code
```c
fl_value_t result = fl_runtime_eval(runtime, "let x = 42;");
```

### Execute File
```c
fl_value_t result = fl_runtime_exec_file(runtime, "program.fl");
```

### Manage Global Variables
```c
// Set global
fl_value_t val;
val.type = FL_TYPE_INT;
val.data.int_val = 42;
fl_runtime_set_global(runtime, "x", val);

// Get global
fl_value_t val = fl_runtime_get_global(runtime, "x");
```

### Check Errors
```c
fl_error_t *error = fl_runtime_get_error(runtime);
if (error) {
    printf("Error: %s\n", error->message);
}
fl_runtime_clear_error(runtime);
```

### Cleanup
```c
fl_runtime_destroy(runtime);
```

---

## File Structure

```
freelang-c/
├── bin/
│   └── fl                  # Compiled executable
├── include/
│   ├── runtime.h           # Runtime interface
│   ├── lexer.h             # Lexer interface
│   ├── parser.h            # Parser interface
│   ├── compiler.h          # Compiler interface
│   ├── vm.h                # VM interface
│   └── ...
├── src/
│   ├── main.c              # CLI entry point
│   ├── runtime.c           # Pipeline implementation
│   ├── lexer.c             # Tokenization
│   ├── parser.c            # AST generation
│   ├── compiler.c          # Bytecode compilation
│   ├── vm.c                # Bytecode execution
│   └── ...
├── obj/                    # Compiled object files
├── Makefile                # Build configuration
└── PHASE3C_RUNTIME_COMPLETE.md
```

---

## Troubleshooting

### Build Fails
```bash
make clean
make setup
make all
```

### Binary Not Found
```bash
# Check if bin/fl exists
ls -la bin/fl

# Rebuild if missing
make all
```

### Runtime Errors
- Check syntax of .fl file
- Look for error messages with line numbers
- Ensure file exists and is readable

### REPL Not Working
```bash
./bin/fl repl
# Type valid FreeLang code
# Use 'exit' to quit
```

---

## Performance Notes

- **Lexer**: O(n) where n = source length
- **Parser**: O(n) with recursive descent
- **Compiler**: O(m) where m = AST nodes
- **VM**: O(k) where k = bytecode instructions

Total: Linear time compilation and execution

---

## Next Steps

1. **Test the runtime**: `./bin/fl run examples/hello.fl`
2. **Try REPL**: `./bin/fl repl`
3. **Review Phase 3-D**: Standard library implementation
4. **Contribute**: Add builtin functions and operators

---

## Reference

- **Phase**: 3-C (Runtime Pipeline)
- **Status**: ✅ Complete and tested
- **Date**: 2026-03-06
- **Build**: Clean (no errors)
- **Compiler**: GCC with -O2 optimizations

---

For detailed information, see `PHASE3C_RUNTIME_COMPLETE.md`
