# Phase 7: Closures & Higher-Order Functions
**FreeLang C Runtime - Implementation Plan**

**Status**: ✅ Infrastructure Setup Complete (2026-03-06)
**Target**: 30-50% completion (Closure basics + function passing)

---

## 📋 Project Overview

Phase 7 implements lexical closures and higher-order function support, enabling:
- Functions that capture variables from their defining scope
- Functions as first-class values (can be stored, passed, returned)
- Map/filter/reduce with closure callbacks
- Nested function definitions with proper scoping

---

## ✅ Infrastructure Completed

### 1. Type System Extensions
- **File**: `include/freelang.h`
- Added `FL_TYPE_CLOSURE` type
- Created `fl_closure_t` structure with:
  - Reference to underlying function
  - Captured variables array (`fl_captured_var_t`)
  - Flexible capacity for variable expansion
- Updated `fl_value_t` union to include closure variant

### 2. New Opcodes
- **File**: `include/freelang.h`
- `FL_OP_MAKE_CLOSURE`: Create closure from function + environment
- `FL_OP_LOAD_UPVALUE`: Load captured variable
- `FL_OP_STORE_UPVALUE`: Modify captured variable

### 3. Closure Management Library
- **File**: `include/closure.h` (NEW)
- **File**: `src/closure.c` (NEW)

**Functions Implemented**:
```c
/* Creation/Destruction */
fl_closure_t* fl_closure_create(fl_function_t* func, ...)
void fl_closure_destroy(fl_closure_t* closure)

/* Variable Capture */
int fl_closure_capture_var(fl_closure_t* closure, const char* name, fl_value_t value)
int fl_closure_get_var(fl_closure_t* closure, const char* name, fl_value_t* out_value)
int fl_closure_set_var(fl_closure_t* closure, const char* name, fl_value_t value)

/* Type Conversions */
fl_value_t fl_value_from_closure(fl_closure_t* closure)
fl_closure_t* fl_value_to_closure(fl_value_t value)
int fl_value_is_closure(fl_value_t value)

/* Higher-Order Support */
fl_value_t fl_closure_call(fl_closure_t* closure, fl_value_t* args, size_t arg_count)
int fl_value_is_callable(fl_value_t value)
fl_value_t fl_value_call(fl_value_t callable, fl_value_t* args, size_t arg_count)
```

---

## 🔄 Implementation Roadmap

### Phase 7-A: Arrow Function Parsing (70% complete)
**Target**: Add arrow function syntax to parser

**Status**:
- [ ] Arrow function tokens (=>) in lexer
- [ ] Parse parenthesized parameters: `(x, y) => x + y`
- [ ] Parse single parameter: `x => x + 1`
- [ ] Parse expression body: `fn f = (x) => x * 2`
- [ ] Parse block body: `fn f = (x) => { return x * 2 }`
- [ ] Update NODE_ARROW_FN AST structure
- [ ] Add to parse_expression() precedence

**Example Syntax**:
```c
// Single param, expression body
let f = x => x + 1;

// Multiple params
let add = (x, y) => x + y;

// Block body
let compute = (x) => {
    let result = x * 2;
    return result;
};
```

### Phase 7-B: Variable Capture (0% complete)
**Target**: Implement lexical environment capture in compiler

**Steps**:
1. **Scope Analysis**: Track which variables are captured
   - When compiling arrow function body, identify upvalue references
   - Mark variables in parent scope as "captured"
   - Store upvalue index in bytecode

2. **Compile Arrow Functions**:
   - Create intermediate function object with bytecode
   - Generate `FL_OP_MAKE_CLOSURE` with captured vars
   - Push closure onto stack

3. **Variable Access**:
   - Local variables: `FL_OP_LOAD_LOCAL` / `FL_OP_STORE_LOCAL`
   - Captured variables: `FL_OP_LOAD_UPVALUE` / `FL_OP_STORE_UPVALUE`
   - Global variables: `FL_OP_LOAD_GLOBAL` / `FL_OP_STORE_GLOBAL`

**Example**:
```c
// Compile this:
let x = 5;
let f = (y) => x + y;

// Generates:
PUSH_INT 5
STORE_LOCAL 0 (x)
LOAD_LOCAL 0 (x)
MAKE_CLOSURE with [x=5]
STORE_LOCAL 1 (f)
```

### Phase 7-C: Closure Execution in VM (0% complete)
**Target**: Execute closures with captured environment

**VM Enhancement**:
1. **Closure Frame Stack**:
   - Each closure call creates closure frame
   - Frame contains upvalues (captured vars)
   - Separate from regular call frames

2. **Opcode Implementation**:
   - `FL_OP_MAKE_CLOSURE`: Create closure_t, capture vars from stack
   - `FL_OP_LOAD_UPVALUE`: Pop closure, load variable by index
   - `FL_OP_STORE_UPVALUE`: Pop closure, store variable by index

3. **Call Protocol**:
   - When calling closure: push args, `FL_OP_CALL`
   - VM recognizes closure type, sets up upvalue frame
   - Execute bytecode with access to captured vars

### Phase 7-D: Higher-Order Functions (0% complete)
**Target**: Support functions as values and callbacks

**Implementations**:
1. **stdlib map/filter/reduce**:
   - Already have stubs in `src/stdlib.c`
   - Extend to accept closure callbacks
   - Example:
     ```c
     let nums = [1, 2, 3, 4, 5];
     let evens = filter(nums, (x) => x % 2 == 0);
     // [2, 4]
     ```

2. **Function First-Class Values**:
   - `fn f = (x) => x + 1;` → stores closure in variable
   - `fn map_over = (fn, arr) => { ... }` → function param
   - `fn make_adder(x) { return (y) => x + y; }` → return closure

3. **Nested Functions**:
   - Functions defined inside functions
   - Each level captures parent scope
   - Multiple nesting levels

### Phase 7-E: Memory Management (10% complete)
**Target**: Proper GC integration for closures

**Status**:
- [x] `fl_closure_destroy()` implemented
- [ ] Register closures with GC
- [ ] Mark/sweep for closure references
- [ ] Update GC to track upvalue arrays
- [ ] Test for memory leaks

### Phase 7-F: Testing & Examples (0% complete)
**Target**: Comprehensive test suite

**Test Files**:
- `examples/closure.fl` - Basic closure example
- `examples/closure_capture.fl` - Multi-level capture
- `examples/higher_order.fl` - map/filter/reduce
- `examples/closure_comprehensive.fl` - All features

---

## 📊 Current Code Statistics

| Component | Lines | Status |
|-----------|-------|--------|
| `include/closure.h` | 90 | ✅ Complete |
| `src/closure.c` | 230 | ✅ Complete (basic) |
| Parser changes | TBD | ⏳ Pending |
| Compiler changes | TBD | ⏳ Pending |
| VM changes | TBD | ⏳ Pending |
| Tests | TBD | ⏳ Pending |

**Total Project**: ~500-800 lines estimated

---

## 🎯 Success Criteria

### Phase 7-A ✅ (Arrow Parsing)
- [ ] Parse `x => x + 1`
- [ ] Parse `(x, y) => x + y`
- [ ] Parse `(x) => { return x * 2 }`
- [ ] No parser errors on closure syntax

### Phase 7-B ✅ (Capture)
- [ ] Identify captured variables in parser/compiler
- [ ] Generate `FL_OP_MAKE_CLOSURE` correctly
- [ ] Store function + upvalues

### Phase 7-C ✅ (VM Execution)
- [ ] `FL_OP_MAKE_CLOSURE` creates fl_closure_t
- [ ] `FL_OP_LOAD_UPVALUE` retrieves captured vars
- [ ] Closures execute with correct environment

### Phase 7-D ✅ (Higher-Order)
- [ ] `map(arr, fn)` works with closure callbacks
- [ ] `filter(arr, fn)` works
- [ ] `reduce(arr, fn)` works

### Phase 7-E ✅ (Memory)
- [ ] No memory leaks on closure creation/destruction
- [ ] GC properly tracks closure references

---

## 📝 Files to Modify

### Core Files
1. **include/token.h** - Add `TOK_ARROW` token type (if not present)
2. **src/lexer.c** - Tokenize `=>` as `TOK_ARROW`
3. **src/parser.c** - Parse arrow function expressions
4. **src/compiler.c** - Compile closures to bytecode
5. **src/vm.c** - Execute closure opcodes
6. **src/stdlib.c** - Update map/filter/reduce for closures
7. **src/runtime.c** - Handle closure values in runtime

### New Files
- ✅ **include/closure.h** - Closure API
- ✅ **src/closure.c** - Closure implementation
- **src/test_closure.c** - Unit tests (NEW)

### Example Files
- **examples/closure.fl** - Basic example
- **examples/closure_capture.fl** - Capture example
- **examples/higher_order.fl** - Higher-order functions
- **examples/closure_comprehensive.fl** - Full test

### Documentation
- ✅ **PHASE7_CLOSURES_PLAN.md** - This file
- **PHASE7_PROGRESS.md** - Weekly progress
- **CLOSURE_IMPLEMENTATION_GUIDE.md** - Technical guide

---

## 🚀 Next Steps

### Immediate (Today)
- [ ] Create closure test examples
- [ ] Implement lexer `=>` tokenization
- [ ] Implement arrow function parsing

### Short Term (Next Session)
- [ ] Implement compiler closure support
- [ ] Implement VM closure opcodes
- [ ] Test basic closure creation and execution

### Medium Term (Following Sessions)
- [ ] Implement capture environment analysis
- [ ] Implement nested function support
- [ ] Update stdlib map/filter/reduce
- [ ] Comprehensive testing and debugging

---

## 💡 Technical Notes

### Closure Representation
```c
Closure = (Function, CapturedEnvironment)

Function = {
    bytecode: [...],
    arity: 2,
    ...
}

CapturedEnvironment = {
    variables: [
        { name: "x", value: 5 },
        { name: "y", value: 10 }
    ]
}
```

### Execution Model
```
Compiler:
  let x = 5;
  let f = (y) => x + y;
  ↓
  PUSH_INT 5
  STORE_LOCAL 0 (x)

  LOAD_LOCAL 0 (get x)
  MAKE_CLOSURE with captured=[x]
  STORE_LOCAL 1 (f)

VM:
  Creates closure_t:
    func = bytecode for "(y) => x + y"
    captured_vars = [{name: "x", value: 5}]

  When calling f(3):
    Pushes args [3]
    Calls closure
    Sets up upvalue frame with x=5
    Executes bytecode with y=3, x=5
    Returns x + y = 8
```

### Capture Semantics
- **Value Capture**: Current approach - captures current values
- **Reference Capture**: Future - captures references for modification
- **Lazy Capture**: Future - capture on use, not on creation

---

## 📚 References

### Related Documentation
- See: `/home/kimjin/Desktop/kim/freelang-c/COMPLETION_CHECKLIST.md`
- See: `/home/kimjin/Desktop/kim/freelang-c/IMPLEMENTATION_SUMMARY.md`
- See: `/home/kimjin/Desktop/kim/freelang-c/include/ast.h` - AST structure

### Similar Implementations
- **Lua**: table-based closures with upvalue chains
- **Python**: cell-based closure objects
- **JavaScript**: lexical scope binding
- **Rust**: trait-based closures with FnOnce/FnMut/Fn

