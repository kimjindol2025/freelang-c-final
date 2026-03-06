# Phase 6: Code Examples & Usage Guide

## Table of Contents
1. [Array Assignment Examples](#array-assignment-examples)
2. [Array Methods Examples](#array-methods-examples)
3. [Lambda/Function Expression Examples](#lambdafunction-expression-examples)
4. [Higher-Order Functions](#higher-order-functions)
5. [Advanced Examples](#advanced-examples)

---

## Array Assignment Examples

### Basic Assignment

```fl
// Create array
let arr = [1, 2, 3];

// Assign to index 0
arr[0] = 10;

// Assign to index 2
arr[2] = 30;

println(arr[0]);  // Output: 10
println(arr[1]);  // Output: 2
println(arr[2]);  // Output: 30
```

**How it works**:
```
Parser: Recognizes arr[0] = 10 as assignment with computed member access
Compiler: Emits: LOAD arr, LOAD 0, LOAD 10, ARRAY_SET
VM: Pops 10, 0, arr from stack, sets arr[0] = 10
```

### Dynamic Index Assignment

```fl
let arr = [1, 2, 3, 4, 5];
let idx = 2;

// Use variable as index
arr[idx] = 100;

println(arr);     // Output: [1, 2, 100, 4, 5]
println(arr[2]);  // Output: 100
```

### Calculated Index

```fl
let arr = [10, 20, 30, 40];

// Use expression as index
arr[1 + 1] = 999;

println(arr[2]);  // Output: 999
```

### Array of Arrays (Nested)

```fl
let matrix = [
    [1, 2, 3],
    [4, 5, 6],
    [7, 8, 9]
];

// Modify nested array element
matrix[1][2] = 60;

println(matrix);  // Output: [[1,2,3], [4,5,60], [7,8,9]]
```

### Loop with Assignment

```fl
let arr = [0, 0, 0, 0, 0];

// Initialize with loop
for i = 0; i < 5; i = i + 1 {
    arr[i] = i * 10;
}

println(arr);  // Output: [0, 10, 20, 30, 40]
```

---

## Array Methods Examples

### push() - Add Element

```fl
let arr = [1, 2, 3];

// Add element to end
push(arr, 4);
push(arr, 5);

println(arr);  // Output: [1, 2, 3, 4, 5]
println(len(arr));  // Output: 5
```

### pop() - Remove Last Element

```fl
let arr = [10, 20, 30, 40];

// Remove and return last element
let last = pop(arr);

println(last);   // Output: 40
println(arr);    // Output: [10, 20, 30]
```

### slice() - Extract Subarray

```fl
let arr = [1, 2, 3, 4, 5];

// Extract elements from index 1 to 3 (exclusive)
let sub = slice(arr, 1, 3);

println(sub);    // Output: [2, 3]
println(arr);    // Output: [1, 2, 3, 4, 5] (original unchanged)
```

### Combining Array Methods

```fl
let numbers = [1, 2, 3];

push(numbers, 4);      // [1, 2, 3, 4]
push(numbers, 5);      // [1, 2, 3, 4, 5]

let first_three = slice(numbers, 0, 3);
println(first_three);  // Output: [1, 2, 3]

let last = pop(numbers);
println(last);         // Output: 5
println(numbers);      // Output: [1, 2, 3, 4]
```

---

## Lambda/Function Expression Examples

### Simple Lambda

```fl
// Function expression assigned to variable
let double = fn(x) {
    return x * 2;
};

// Call the function
println(double(5));    // Output: 10
println(double(100));  // Output: 200
```

### Lambda with Multiple Parameters

```fl
let add = fn(a, b) {
    return a + b;
};

println(add(3, 4));    // Output: 7
println(add(10, 20));  // Output: 30
```

### Lambda Returning Complex Expression

```fl
let calculate = fn(x, y) {
    let sum = x + y;
    let product = x * y;
    return sum + product;
};

// Test: x=3, y=4
// sum = 7, product = 12, return 19
println(calculate(3, 4));  // Output: 19
```

### Lambda with Conditional Logic

```fl
let max = fn(a, b) {
    if a > b {
        return a;
    } else {
        return b;
    }
};

println(max(10, 20));  // Output: 20
println(max(50, 30));  // Output: 50
```

### Lambda with Loop

```fl
let sum = fn(n) {
    let total = 0;
    for i = 1; i <= n; i = i + 1 {
        total = total + i;
    }
    return total;
};

println(sum(5));   // Output: 15 (1+2+3+4+5)
println(sum(10));  // Output: 55 (1+2+...+10)
```

### Multiple Lambdas

```fl
let square = fn(x) { return x * x; };
let cube = fn(x) { return x * x * x; };
let double = fn(x) { return x * 2; };

println(square(4));  // Output: 16
println(cube(3));    // Output: 27
println(double(7));  // Output: 14
```

---

## Higher-Order Functions

### map() - Transform Array Elements

```fl
let numbers = [1, 2, 3, 4, 5];

// Apply function to each element
let doubled = map(numbers, fn(x) {
    return x * 2;
});

println(doubled);  // Output: [2, 4, 6, 8, 10]
```

### map() with More Complex Transformation

```fl
let values = [1, 2, 3];

// Each element squared plus 1
let transformed = map(values, fn(x) {
    return x * x + 1;
});

println(transformed);  // Output: [2, 5, 10]
// 1*1+1=2, 2*2+1=5, 3*3+1=10
```

### Reusable Function with map()

```fl
let triple = fn(x) {
    return x * 3;
};

let arr = [1, 2, 3, 4];
let result = map(arr, triple);

println(result);  // Output: [3, 6, 9, 12]
```

### Chaining Lambdas

```fl
// Double all numbers
let arr = [1, 2, 3];
let step1 = map(arr, fn(x) { return x * 2; });
println(step1);  // Output: [2, 4, 6]

// Then add 1 to each
let step2 = map(step1, fn(x) { return x + 1; });
println(step2);  // Output: [3, 5, 7]
```

---

## Advanced Examples

### Higher-Order Function Builder

```fl
// Function that returns a function
let makeMultiplier = fn(n) {
    return fn(x) {
        return x * n;
    };
};

let double = makeMultiplier(2);
let triple = makeMultiplier(3);

println(double(5));  // Output: 10
println(triple(5));  // Output: 15
```

**Note**: This example may not work in current phase due to closure limitations.
It will be fixed in Phase 7.

### Array Transformation Pipeline

```fl
// Start with array
let numbers = [1, 2, 3, 4, 5];

// Transform 1: Double each element
let doubled = map(numbers, fn(x) { return x * 2; });

// Transform 2: Filter and assign
let arr = doubled;
arr[0] = 10;  // Override first element

println(arr);  // Output: [10, 4, 6, 8, 10]
```

### String Operations with Functions

```fl
let text = "hello";

println(len(text));     // Output: 5
println(upper(text));   // Output: "HELLO"
println(lower("WORLD")); // Output: "world"
```

### Combining Everything

```fl
// Problem: Square numbers 1-5, double them, then sum

// Step 1: Create array [1, 2, 3, 4, 5]
let arr = [1, 2, 3, 4, 5];

// Step 2: Square each
let squared = map(arr, fn(x) { return x * x; });
println(squared);  // Output: [1, 4, 9, 16, 25]

// Step 3: Double each
let doubled = map(squared, fn(x) { return x * 2; });
println(doubled);  // Output: [2, 8, 18, 32, 50]

// Step 4: Sum manually
let total = 0;
for i = 0; i < len(doubled); i = i + 1 {
    total = total + doubled[i];
}
println(total);  // Output: 110
```

---

## Common Patterns

### Pattern 1: Filter with Assignment

```fl
let arr = [1, 2, 3, 4, 5];
let filtered = [];

for i = 0; i < len(arr); i = i + 1 {
    if arr[i] > 2 {
        push(filtered, arr[i]);
    }
}

println(filtered);  // Output: [3, 4, 5]
```

### Pattern 2: Reduce (Manual Implementation)

```fl
let sum = fn(arr) {
    let total = 0;
    for i = 0; i < len(arr); i = i + 1 {
        total = total + arr[i];
    }
    return total;
};

println(sum([1, 2, 3, 4, 5]));  // Output: 15
```

### Pattern 3: Map and Filter Combined

```fl
let numbers = [1, 2, 3, 4, 5];

// Transform
let transformed = map(numbers, fn(x) { return x * x; });

// Filter (keep only those < 20)
let filtered = [];
for i = 0; i < len(transformed); i = i + 1 {
    if transformed[i] < 20 {
        push(filtered, transformed[i]);
    }
}

println(filtered);  // Output: [1, 4, 9, 16]
```

---

## Edge Cases & Gotchas

### Array Bounds

```fl
let arr = [1, 2, 3];

// Valid indices: 0, 1, 2
arr[0] = 10;  // OK
arr[2] = 30;  // OK

// Out of bounds (expands array in current implementation)
arr[5] = 50;  // Creates arr[3]=null, arr[4]=null, arr[5]=50
println(len(arr));  // Output: 6
```

### Lambda Variable Scope

```fl
let x = 10;
let f = fn(y) {
    // Can access parameter y
    // Currently CANNOT access outer x (closure not implemented)
    return y * 2;  // OK
    // return x + y;  // Will fail - x not in scope
};

println(f(5));  // Output: 10
```

### Function Assignment

```fl
let f = fn(x) { return x + 1; };
let g = f;  // g is reference to same function

println(f(5));  // Output: 6
println(g(5));  // Output: 6
```

### Nested Function Calls

```fl
let add = fn(a, b) { return a + b; };
let mul = fn(a, b) { return a * b; };

// Nested: mul(add(2,3), 4) = mul(5, 4) = 20
println(mul(add(2, 3), 4));  // Output: 20
```

---

## Performance Considerations

### Array Assignment is O(1)
```fl
let arr = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
arr[0] = 100;     // Fast: direct index access
arr[9] = 999;     // Same speed
```

### Array.push() is O(1) amortized
```fl
let arr = [];
for i = 0; i < 1000; i = i + 1 {
    push(arr, i);  // Efficient dynamic array growth
}
```

### Array.slice() is O(n) (copies elements)
```fl
let arr = [1, 2, 3, 4, 5];
let sub = slice(arr, 0, 1000);  // Creates new array
```

### map() is O(n)
```fl
let arr = [1, 2, 3, 4, 5];
let result = map(arr, fn(x) { return x * 2; });  // O(n)
```

---

## Testing Checklist

When implementing Phase 6 features, verify:

- [ ] Array assignment works with literal indices
- [ ] Array assignment works with variable indices
- [ ] Array assignment works with expression indices
- [ ] Array methods (push, pop, slice) work correctly
- [ ] Lambda syntax parses correctly
- [ ] Lambdas execute with correct parameter binding
- [ ] Lambdas with multiple parameters work
- [ ] Lambdas with block bodies work
- [ ] map() works with lambdas
- [ ] Nested arrays work
- [ ] Error handling for out-of-bounds (graceful)
- [ ] Function values can be passed and called
- [ ] Lambda names are unique (no collisions)

---

## References

### Stack Order Convention
```
Bottom → Top (direction of push)
[array] [index] [value] ← stack top

FL_OP_ARRAY_SET execution:
1. Pop value (top)
2. Pop index
3. Pop array (bottom)
4. Perform: array[index] = value
```

### Compilation Order for arr[idx] = val
```
1. Compile array expression
2. Compile index expression
3. Compile value expression
4. Emit FL_OP_ARRAY_SET
```

### Function Expression Registration
```
1. Parse: fn(params) { body }
2. Create function object
3. Compile body to bytecode
4. Register in function table at index N
5. Emit: PUSH_INT N (push function reference)
6. At runtime: function value is on stack
```

---

**Document Version**: 1.0
**Last Updated**: 2026-03-06
**Status**: Complete
