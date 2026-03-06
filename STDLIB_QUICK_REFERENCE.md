# FreeLang C Standard Library - Quick Reference

## Overview

The FreeLang C stdlib provides 65+ functions organized into 8 categories. All functions follow the pattern:

```c
fl_value_t fl_category_operation(fl_value_t* args, size_t argc);
```

---

## I/O Functions

```c
fl_println(args, argc)      // print with newline
fl_print(args, argc)        // print without newline
fl_input(args, argc)        // read line from stdin (optional prompt)
```

**Example**:
```c
fl_value_t msg = fl_new_string("Hello");
fl_println(&msg, 1);        // prints "Hello\n"

fl_value_t prompt = fl_new_string("Enter name: ");
fl_value_t input = fl_input(&prompt, 1);
```

---

## String Functions (16 total)

### Length & Search
```c
fl_string_length(args, 1)       // → int (length)
fl_string_index_of(args, 2)     // str, substr → int (index or -1)
fl_string_contains(args, 2)     // str, substr → bool
```

### Manipulation
```c
fl_string_upper(args, 1)        // → uppercase string
fl_string_lower(args, 1)        // → lowercase string
fl_string_trim(args, 1)         // → trimmed string
fl_string_replace(args, 3)      // str, find, replace → new string
fl_string_repeat(args, 2)       // str, count → repeated string
```

### Slicing & Splitting
```c
fl_string_slice(args, 3)        // str, start, end → substring
fl_string_split(args, 2)        // str, delim → string array
fl_string_concat(args, N)       // str1, str2, ... → concatenated
```

### Padding & Substring
```c
fl_string_char_at(args, 2)      // str, index → single char string
fl_string_pad_start(args, 3)    // str, len, pad → padded string
fl_string_pad_end(args, 3)      // str, len, pad → padded string
fl_string_starts_with(args, 2)  // str, prefix → bool
fl_string_ends_with(args, 2)    // str, suffix → bool
```

**Example**:
```c
fl_value_t str1 = fl_new_string("hello world");
fl_value_t str2 = fl_new_string("world");
fl_value_t result = fl_string_contains(&str1, 1);  // true
```

---

## Array Functions (17 total)

### Basic Operations
```c
fl_array_length(args, 1)        // → array size
fl_array_push(args, 2)          // array, elem → new length
fl_array_pop(args, 1)           // → last element
fl_array_shift(args, 1)         // → first element
fl_array_unshift(args, 2)       // array, elem → new length
```

### Search & Access
```c
fl_array_index_of(args, 2)      // array, elem → int (index or -1)
fl_array_last_index_of(args, 2) // array, elem → int (last index)
fl_array_includes(args, 2)      // array, elem → bool
fl_array_slice(args, 3)         // array, start, end → subarray
```

### Transformation
```c
fl_array_reverse(args, 1)       // → reversed array (in-place)
fl_array_sort(args, 1)          // → sorted array (numeric/string)
fl_array_join(args, 2)          // array, separator → joined string
```

### Higher-Order (Stubs - require function pointers)
```c
fl_array_map(args, 2)           // array, fn → mapped array
fl_array_filter(args, 2)        // array, predicate → filtered array
fl_array_reduce(args, 3)        // array, fn, init → accumulated value
```

**Example**:
```c
fl_array_t* arr = fl_array_create(10);
fl_value_t arr_val;
arr_val.type = FL_TYPE_ARRAY;
arr_val.data.array_val = arr;

fl_value_t elem = fl_new_int(42);
fl_array_push(&arr_val, 2);  // adds 42, returns new length
```

---

## Math Functions (15 total)

### Basic
```c
fl_math_abs(args, 1)            // → absolute value
fl_math_min(args, N)            // num1, num2, ... → minimum
fl_math_max(args, N)            // num1, num2, ... → maximum
```

### Rounding
```c
fl_math_floor(args, 1)          // → rounded down
fl_math_ceil(args, 1)           // → rounded up
fl_math_round(args, 1)          // → nearest integer
```

### Power & Root
```c
fl_math_sqrt(args, 1)           // → square root
fl_math_pow(args, 2)            // base, exp → power
```

### Trigonometric
```c
fl_math_sin(args, 1)            // → sine (radians)
fl_math_cos(args, 1)            // → cosine (radians)
fl_math_tan(args, 1)            // → tangent (radians)
```

### Logarithmic
```c
fl_math_log(args, 1)            // → natural logarithm
fl_math_exp(args, 1)            // → e^x
```

### Random
```c
fl_math_random(args, 0)         // → float [0, 1)
```

**Example**:
```c
fl_value_t x = fl_new_float(3.14159);
fl_value_t sin_x = fl_math_sin(&x, 1);  // ≈ 0
```

**Compile with**: `gcc ... -lm`

---

## Type Conversion (6 total)

### Introspection
```c
fl_typeof(args, 1)              // → type name string
fl_is_null(args, 1)             // → bool
```

### Conversion
```c
fl_string_convert(args, 1)      // any → string
fl_number_convert(args, 1)      // string/bool/num → float
fl_bool_convert(args, 1)        // any → bool
fl_array_from(args, 1)          // any → array
```

**Truthiness**:
- `null` → false
- `0` / `0.0` / `""` → false
- Everything else → true

**Example**:
```c
fl_value_t num = fl_new_int(42);
fl_value_t str = fl_string_convert(&num, 1);
// str.data.string_val == "42"
```

---

## Object Functions (4 total)

### Inspection
```c
fl_object_keys(args, 1)         // → string array
fl_object_values(args, 1)       // → value array
fl_object_entries(args, 1)      // → [key, value][] array
```

### Manipulation
```c
fl_object_assign(args, N)       // target, src1, src2, ... → target
```

**Example**:
```c
fl_object_t* obj = fl_object_create(10);
// ... populate obj ...
fl_value_t obj_val;
obj_val.type = FL_TYPE_OBJECT;
obj_val.data.object_val = obj;

fl_value_t keys = fl_object_keys(&obj_val, 1);
// keys is array of strings
```

---

## JSON Functions (2 total)

### Serialization
```c
fl_json_stringify(args, 1)      // any → JSON string
```

### Parsing
```c
fl_json_parse(args, 1)          // JSON string → value (stub)
```

**Example**:
```c
fl_value_t obj_val = /* ... object ... */;
fl_value_t json = fl_json_stringify(&obj_val, 1);
printf("%s\n", json.data.string_val);
// Output: {"key":"value",...}
```

---

## Control Flow (2 total)

```c
fl_assert(args, 1)              // cond, [msg] → void (exits if false)
fl_panic(args, 1)               // [msg] → void (always exits)
```

**Example**:
```c
fl_value_t cond = fl_new_bool(false);
fl_value_t msg = fl_new_string("failed");
fl_value_t args[] = {cond, msg};
fl_assert(args, 2);             // exits with "AssertionError: failed"
```

---

## Memory Management

### Important Notes

1. **String Allocation**: All string-returning functions allocate new memory
   ```c
   fl_value_t result = fl_string_upper(&input, 1);
   free(result.data.string_val);  // Must free!
   ```

2. **Array Expansion**: Push/unshift auto-expand capacity (2x growth)
   ```c
   fl_array_push(&arr, 2);  // O(1) amortized
   ```

3. **Type Checking**: Invalid argument types return `FL_TYPE_NULL`
   ```c
   fl_value_t bad = fl_new_int(42);
   fl_value_t result = fl_string_length(&bad, 1);
   // result.type == FL_TYPE_NULL
   ```

---

## Common Patterns

### String Building
```c
fl_value_t parts[] = {
    fl_new_string("Hello"),
    fl_new_string(" "),
    fl_new_string("World")
};
fl_value_t result = fl_string_concat(parts, 3);
// result.data.string_val == "Hello World"
free(result.data.string_val);
```

### Array Processing
```c
// Filter by value
fl_array_t* filtered = fl_array_create(10);
for (size_t i = 0; i < arr->size; i++) {
    if (arr->elements[i].data.int_val > 5) {
        // Add to filtered
    }
}

// Or use fl_array_slice for sublists
fl_value_t slice = fl_array_slice(&arr_val, 3, 7);  // elements 3-6
```

### Type Detection
```c
fl_value_t type_name = fl_typeof(&value, 1);
if (strcmp(type_name.data.string_val, "string") == 0) {
    // Handle string
} else if (strcmp(type_name.data.string_val, "array") == 0) {
    // Handle array
}
free(type_name.data.string_val);
```

---

## Error Handling Strategy

**All functions return `FL_TYPE_NULL` on error**:
- Invalid argument count
- Type mismatch
- Out of bounds access
- Memory allocation failure

```c
fl_value_t result = fl_array_index_of(&non_array, 1);
if (result.type == FL_TYPE_NULL) {
    // Handle error
}
```

---

## Performance Tips

1. **String Operations**: `slice` and `replace` allocate; use wisely in loops
2. **Array Sorting**: Uses `qsort()`, O(n log n); not stable
3. **String Search**: `contains` uses `strstr()`, O(n*m)
4. **Array Access**: Direct indexing O(1), but no bounds checking outside push/pop

---

## Next Steps for Integration

1. **Register Functions**: Populate `fl_stdlib_register()` to expose to VM
2. **VM Integration**: Wire builtin calls to these functions
3. **Test Suite**: Create test cases for all 65 functions
4. **Documentation**: Add these functions to language manual

---

**Last Updated**: 2026-03-06
**FreeLang C Runtime - Phase 3-B**
