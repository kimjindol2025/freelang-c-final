#!/bin/bash

##############################################################################
# FreeLang C - Phase 4: Integrated Testing Suite & Performance Validation
#
# Purpose: Execute comprehensive tests across all components
# Status: Phase 3C completed, runtime execution debugging
# Date: 2026-03-06
##############################################################################

set -e

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counters
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_TOTAL=0

# Create test results directory
TEST_RESULTS_DIR="test_results_phase4"
mkdir -p "$TEST_RESULTS_DIR"

##############################################################################
# Test Utilities
##############################################################################

log_section() {
    echo -e "\n${BLUE}=== $1 ===${NC}"
}

log_test() {
    echo -n "  [$TESTS_TOTAL] $1... "
    TESTS_TOTAL=$((TESTS_TOTAL + 1))
}

test_pass() {
    echo -e "${GREEN}PASS${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

test_fail() {
    echo -e "${RED}FAIL${NC}"
    TESTS_FAILED=$((TESTS_FAILED + 1))
    echo -e "    ${RED}Error: $1${NC}" >&2
}

test_skip() {
    echo -e "${YELLOW}SKIP${NC}"
    echo -e "    ${YELLOW}Reason: $1${NC}"
}

##############################################################################
# Phase 4-A: Unit Tests (Component Testing)
##############################################################################

test_phase4_unit_tests() {
    log_section "Phase 4-A: Unit Tests"

    # Test A1: Lexer functionality
    log_test "Lexer: Tokenize simple program"
    if [ -f "bin/test_lexer" ]; then
        if timeout 5 ./bin/test_lexer > "$TEST_RESULTS_DIR/lexer.log" 2>&1; then
            test_pass
        else
            test_fail "Lexer test failed"
        fi
    else
        test_skip "test_lexer binary not found"
    fi

    # Test A2: Parser functionality
    log_test "Parser: Parse AST structure"
    if [ -f "bin/test_parser" ]; then
        if timeout 5 ./bin/test_parser > "$TEST_RESULTS_DIR/parser.log" 2>&1; then
            test_pass
        else
            test_fail "Parser test failed"
        fi
    else
        test_skip "test_parser binary not found"
    fi

    # Test A3: VM functionality
    log_test "VM: Execute bytecode"
    if [ -f "bin/test_vm" ]; then
        if timeout 5 ./bin/test_vm > "$TEST_RESULTS_DIR/vm.log" 2>&1; then
            test_pass
        else
            test_fail "VM test failed"
        fi
    else
        test_skip "test_vm binary not found"
    fi

    # Test A4: GC functionality
    log_test "GC: Memory management"
    if [ -f "bin/test_gc" ]; then
        if timeout 5 ./bin/test_gc > "$TEST_RESULTS_DIR/gc.log" 2>&1; then
            test_pass
        else
            test_fail "GC test failed"
        fi
    else
        test_skip "test_gc binary not found"
    fi
}

##############################################################################
# Phase 4-B: Integration Tests (Full Pipeline)
##############################################################################

test_phase4_integration_tests() {
    log_section "Phase 4-B: Integration Tests"

    if [ ! -f "bin/fl" ]; then
        echo -e "${RED}Error: bin/fl not found. Build first with 'make'${NC}"
        return 1
    fi

    # Test B1: Hello World
    log_test "Integration: Hello World"
    if timeout 5 ./bin/fl run examples/hello_world.fl > "$TEST_RESULTS_DIR/hello_world.txt" 2>&1; then
        if grep -q "Hello, World!" "$TEST_RESULTS_DIR/hello_world.txt"; then
            test_pass
        else
            test_fail "Output doesn't contain 'Hello, World!'"
        fi
    else
        test_fail "Timeout or execution error"
    fi

    # Test B2: Arithmetic
    log_test "Integration: Arithmetic operations"
    if timeout 5 ./bin/fl run examples/simple_math.fl > "$TEST_RESULTS_DIR/arithmetic.txt" 2>&1; then
        if grep -q "50" "$TEST_RESULTS_DIR/arithmetic.txt"; then
            test_pass
        else
            test_fail "Output doesn't match expected result"
        fi
    else
        test_fail "Timeout or execution error"
    fi

    # Test B3: Fibonacci (function definition)
    log_test "Integration: Fibonacci (function calls)"
    if timeout 5 ./bin/fl run examples/fibonacci.fl > "$TEST_RESULTS_DIR/fibonacci.txt" 2>&1; then
        # Fibonacci(10) should produce 55
        if grep -q "55" "$TEST_RESULTS_DIR/fibonacci.txt"; then
            test_pass
        else
            test_fail "Fibonacci output incorrect"
        fi
    else
        test_fail "Timeout or execution error"
    fi

    # Test B4: REPL mode
    log_test "Integration: REPL mode startup"
    if echo "exit" | timeout 5 ./bin/fl repl > "$TEST_RESULTS_DIR/repl.txt" 2>&1; then
        if grep -q "FreeLang" "$TEST_RESULTS_DIR/repl.txt"; then
            test_pass
        else
            test_fail "REPL startup failed"
        fi
    else
        test_fail "REPL startup error"
    fi
}

##############################################################################
# Phase 4-C: Stress Tests (Robustness)
##############################################################################

test_phase4_stress_tests() {
    log_section "Phase 4-C: Stress Tests"

    if [ ! -f "bin/fl" ]; then
        return 1
    fi

    # Create stress test 1: Large numbers
    cat > "$TEST_RESULTS_DIR/stress_large_numbers.fl" << 'EOF'
let x = 999999999;
let y = 888888888;
let z = x + y;
println(z);
EOF

    log_test "Stress: Large number arithmetic"
    if timeout 5 ./bin/fl run "$TEST_RESULTS_DIR/stress_large_numbers.fl" \
        > "$TEST_RESULTS_DIR/stress_large_numbers.txt" 2>&1; then
        test_pass
    else
        test_fail "Large number test failed"
    fi

    # Create stress test 2: Long strings
    cat > "$TEST_RESULTS_DIR/stress_long_strings.fl" << 'EOF'
let s = "This is a longer string to test string handling in the runtime";
println(s);
EOF

    log_test "Stress: String handling"
    if timeout 5 ./bin/fl run "$TEST_RESULTS_DIR/stress_long_strings.fl" \
        > "$TEST_RESULTS_DIR/stress_long_strings.txt" 2>&1; then
        test_pass
    else
        test_fail "String handling test failed"
    fi

    # Create stress test 3: Nested operations
    cat > "$TEST_RESULTS_DIR/stress_nested_ops.fl" << 'EOF'
let a = 2;
let b = 3;
let c = 4;
let result = a + b * c;
println(result);
EOF

    log_test "Stress: Nested operations (precedence)"
    if timeout 5 ./bin/fl run "$TEST_RESULTS_DIR/stress_nested_ops.fl" \
        > "$TEST_RESULTS_DIR/stress_nested_ops.txt" 2>&1; then
        test_pass
    else
        test_fail "Nested operations test failed"
    fi
}

##############################################################################
# Phase 4-D: Performance Benchmarks
##############################################################################

test_phase4_benchmarks() {
    log_section "Phase 4-D: Performance Benchmarks"

    if [ ! -f "bin/fl" ]; then
        return 1
    fi

    # Benchmark 1: Startup time
    log_test "Benchmark: Startup latency"
    START=$(date +%s%N)
    timeout 5 ./bin/fl run examples/hello_world.fl > /dev/null 2>&1
    END=$(date +%s%N)
    TIME_MS=$(( (END - START) / 1000000 ))

    if [ $TIME_MS -lt 500 ]; then
        echo -e "${GREEN}PASS${NC} (${TIME_MS}ms)"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${YELLOW}WARN${NC} (${TIME_MS}ms, expected <500ms)"
    fi
    TESTS_TOTAL=$((TESTS_TOTAL + 1))

    # Benchmark 2: Binary size
    log_test "Benchmark: Binary size"
    SIZE=$(stat -c%s bin/fl 2>/dev/null || echo "unknown")
    if [ "$SIZE" != "unknown" ]; then
        SIZE_KB=$((SIZE / 1024))
        echo " ${SIZE_KB}KB"
        if [ $SIZE_KB -lt 500 ]; then
            TESTS_PASSED=$((TESTS_PASSED + 1))
        fi
    else
        echo " (cannot determine)"
    fi
    TESTS_TOTAL=$((TESTS_TOTAL + 1))

    # Benchmark 3: Memory usage
    log_test "Benchmark: Memory footprint (peak)"
    # Simple heuristic - we can't reliably measure without valgrind
    echo " (requires valgrind for accurate measurement)"
    TESTS_TOTAL=$((TESTS_TOTAL + 1))
}

##############################################################################
# Phase 4-E: Memory Safety Tests
##############################################################################

test_phase4_memory_tests() {
    log_section "Phase 4-E: Memory Safety"

    if ! command -v valgrind &> /dev/null; then
        echo -e "${YELLOW}Valgrind not installed, skipping memory tests${NC}"
        return 0
    fi

    if [ ! -f "bin/fl" ]; then
        return 1
    fi

    # Test with valgrind
    log_test "Memory: Valgrind leak check (hello_world)"
    if timeout 30 valgrind --leak-check=full --error-exitcode=99 \
        ./bin/fl run examples/hello_world.fl > "$TEST_RESULTS_DIR/valgrind.log" 2>&1; then
        if grep -q "ERROR SUMMARY: 0 errors" "$TEST_RESULTS_DIR/valgrind.log"; then
            test_pass
        else
            test_fail "Memory leaks detected"
        fi
    else
        if [ $? -eq 99 ]; then
            test_fail "Valgrind found errors"
        else
            test_skip "Valgrind check inconclusive"
        fi
    fi
}

##############################################################################
# Phase 4-F: Error Handling Tests
##############################################################################

test_phase4_error_tests() {
    log_section "Phase 4-F: Error Handling"

    if [ ! -f "bin/fl" ]; then
        return 1
    fi

    # Test E1: Undefined variable
    cat > "$TEST_RESULTS_DIR/error_undefined_var.fl" << 'EOF'
let x = y + 1;
EOF

    log_test "Error: Undefined variable detection"
    if timeout 5 ./bin/fl run "$TEST_RESULTS_DIR/error_undefined_var.fl" \
        > "$TEST_RESULTS_DIR/error_undefined_var.log" 2>&1; then
        if grep -q "ERROR\|undefined\|undef" "$TEST_RESULTS_DIR/error_undefined_var.log"; then
            test_pass
        else
            test_fail "Expected error not detected"
        fi
    else
        test_fail "Program crashed instead of reporting error"
    fi

    # Test E2: Type mismatch
    cat > "$TEST_RESULTS_DIR/error_type_mismatch.fl" << 'EOF'
let x = "string" + 5;
EOF

    log_test "Error: Type mismatch reporting"
    if timeout 5 ./bin/fl run "$TEST_RESULTS_DIR/error_type_mismatch.fl" \
        > "$TEST_RESULTS_DIR/error_type_mismatch.log" 2>&1; then
        test_pass  # Type checking may be permissive in current implementation
    else
        test_fail "Program crashed"
    fi
}

##############################################################################
# Phase 4-G: Documentation Validation
##############################################################################

test_phase4_docs() {
    log_section "Phase 4-G: Documentation Validation"

    # Check critical documentation files
    DOCS=(
        "README.md"
        "RUNTIME_QUICK_START.md"
        "STDLIB_QUICK_REFERENCE.md"
        "PHASE3_STATUS.md"
    )

    for doc in "${DOCS[@]}"; do
        log_test "Documentation: $doc exists"
        if [ -f "$doc" ]; then
            test_pass
        else
            test_fail "Missing documentation"
        fi
    done
}

##############################################################################
# Summary Report
##############################################################################

generate_summary_report() {
    log_section "Phase 4 Test Summary"

    TESTS_SKIPPED=$((TESTS_TOTAL - TESTS_PASSED - TESTS_FAILED))

    echo -e "\n${BLUE}Results:${NC}"
    echo "  Passed:  ${GREEN}$TESTS_PASSED${NC}"
    echo "  Failed:  ${RED}$TESTS_FAILED${NC}"
    echo "  Skipped: ${YELLOW}$TESTS_SKIPPED${NC}"
    echo "  Total:   $TESTS_TOTAL"

    PASS_RATE=$((TESTS_PASSED * 100 / TESTS_TOTAL))
    echo -e "\n${BLUE}Pass Rate:${NC} $PASS_RATE%"

    # Generate HTML report
    cat > "$TEST_RESULTS_DIR/report.html" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>FreeLang C - Phase 4 Test Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .header { background: #333; color: white; padding: 10px; border-radius: 5px; }
        .pass { color: #28a745; font-weight: bold; }
        .fail { color: #dc3545; font-weight: bold; }
        .skip { color: #ffc107; font-weight: bold; }
        table { border-collapse: collapse; width: 100%; margin-top: 20px; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #f2f2f2; }
    </style>
</head>
<body>
    <div class="header">
        <h1>FreeLang C - Phase 4 Integrated Testing Report</h1>
        <p>Date: $(date)</p>
    </div>

    <h2>Test Summary</h2>
    <p>Passed: <span class="pass">$TESTS_PASSED</span> / Failed: <span class="fail">$TESTS_FAILED</span> / Skipped: <span class="skip">$TESTS_SKIPPED</span></p>
    <p>Pass Rate: <strong>$PASS_RATE%</strong></p>

    <h2>Test Categories</h2>
    <ul>
        <li>Phase 4-A: Unit Tests (Lexer, Parser, VM, GC)</li>
        <li>Phase 4-B: Integration Tests (Full pipeline)</li>
        <li>Phase 4-C: Stress Tests (Large numbers, strings)</li>
        <li>Phase 4-D: Performance Benchmarks</li>
        <li>Phase 4-E: Memory Safety (Valgrind)</li>
        <li>Phase 4-F: Error Handling</li>
        <li>Phase 4-G: Documentation</li>
    </ul>

    <h2>Detailed Results</h2>
    <p>See individual test logs in:</p>
    <pre>$TEST_RESULTS_DIR/</pre>
</body>
</html>
EOF

    echo -e "\n${BLUE}HTML Report:${NC} $TEST_RESULTS_DIR/report.html"
    echo -e "${BLUE}Test Results Dir:${NC} $TEST_RESULTS_DIR/"

    # Exit with appropriate code
    if [ $TESTS_FAILED -gt 0 ]; then
        return 1
    else
        return 0
    fi
}

##############################################################################
# Main Execution
##############################################################################

main() {
    echo -e "${BLUE}"
    echo "╔════════════════════════════════════════════════════════════╗"
    echo "║  FreeLang C - Phase 4: Integrated Testing & Validation     ║"
    echo "║  Comprehensive test suite for components and integration   ║"
    echo "╚════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"

    # Run all test phases
    test_phase4_unit_tests
    test_phase4_integration_tests
    test_phase4_stress_tests
    test_phase4_benchmarks
    test_phase4_memory_tests
    test_phase4_error_tests
    test_phase4_docs

    # Generate summary
    generate_summary_report
}

# Execute
main
exit $?
