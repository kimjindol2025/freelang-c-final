/**
 * FreeLang Performance Benchmarks
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "../include/freelang.h"

#define ITERATIONS 10000

/* Utility function to measure time */
typedef struct {
    clock_t start;
    clock_t end;
    double elapsed;
} timer_t;

timer_t timer_start(void) {
    timer_t t;
    t.start = clock();
    t.end = 0;
    t.elapsed = 0;
    return t;
}

void timer_stop(timer_t* t) {
    t->end = clock();
    t->elapsed = (double)(t->end - t->start) / CLOCKS_PER_SEC;
}

void timer_print(const char* name, timer_t* t) {
    printf("%-30s: %10.6f seconds\n", name, t->elapsed);
}

/* Benchmark functions */

void bench_arithmetic(void) {
    printf("\n--- Arithmetic Operations ---\n");
    timer_t t = timer_start();

    volatile long result = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        result += (i * 2) - (i / 2);
    }

    timer_stop(&t);
    timer_print("Arithmetic (10k ops)", &t);
}

void bench_string_concat(void) {
    printf("\n--- String Operations ---\n");
    timer_t t = timer_start();

    /* TODO: Implement string benchmark */
    timer_stop(&t);
    timer_print("String concatenation (1k ops)", &t);
}

void bench_array_ops(void) {
    printf("\n--- Array Operations ---\n");
    timer_t t = timer_start();

    /* TODO: Implement array benchmark */
    timer_stop(&t);
    timer_print("Array operations (1k ops)", &t);
}

void bench_function_calls(void) {
    printf("\n--- Function Calls ---\n");
    timer_t t = timer_start();

    /* TODO: Implement function call benchmark */
    timer_stop(&t);
    timer_print("Function calls (10k calls)", &t);
}

void bench_recursion(void) {
    printf("\n--- Recursion ---\n");
    timer_t t = timer_start();

    /* TODO: Implement recursion benchmark */
    timer_stop(&t);
    timer_print("Fibonacci (fib(30))", &t);
}

void bench_loop_performance(void) {
    printf("\n--- Loop Performance ---\n");
    timer_t t = timer_start();

    volatile long count = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        count++;
    }

    timer_stop(&t);
    timer_print("Simple loop (10k iterations)", &t);
}

int main(void) {
    printf("========================================\n");
    printf("FreeLang C Runtime - Performance Benchmarks\n");
    printf("========================================\n");

    bench_arithmetic();
    bench_loop_performance();
    bench_string_concat();
    bench_array_ops();
    bench_function_calls();
    bench_recursion();

    printf("\n========================================\n");
    printf("Benchmark complete\n");
    printf("========================================\n");

    return 0;
}
