/**
 * FreeLang C Runtime - Main Entry Point
 * Phase 3-C: Complete Runtime Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/runtime.h"
#include "../include/ast.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "FreeLang v1.0.0\n");
        fprintf(stderr, "Usage: %s <command> [args]\n", argv[0]);
        fprintf(stderr, "Commands:\n");
        fprintf(stderr, "  run <file>     - Execute a FreeLang file\n");
        fprintf(stderr, "  repl           - Start interactive REPL\n");
        fprintf(stderr, "  test           - Run AST tests\n");
        return 1;
    }

    const char *cmd = argv[1];

    /* Check if argv[1] is a file (implicit run command) */
    int is_file = 0;
    if (cmd[0] != '-' && strchr(cmd, '.')) {
        /* Likely a filename like "examples/hello.fl" */
        is_file = 1;
    }

    /* ========================================
       RUN COMMAND: Execute a file
       ======================================== */
    if (strcmp(cmd, "run") == 0 || is_file) {
        const char *filename;
        if (is_file) {
            filename = argv[1];
        } else {
            if (argc < 3) {
                fprintf(stderr, "Error: 'run' requires a filename\n");
                return 1;
            }
            filename = argv[2];
        }

        /* Initialize AST pool */
        ast_init_pool(10000);

        /* Create runtime */
        fl_runtime_t *runtime = fl_runtime_create();
        if (!runtime) {
            fprintf(stderr, "Error: Runtime creation failed\n");
            ast_free_all();
            return 1;
        }

        /* Execute file */
        fprintf(stderr, "[DEBUG] Executing file: %s\n", filename);
        fl_value_t result = fl_runtime_exec_file(runtime, filename);
        fprintf(stderr, "[DEBUG] Execution complete\n");

        /* Check for errors */
        fl_error_t *error = fl_runtime_get_error(runtime);
        if (error && error->message) {
            fprintf(stderr, "ERROR [%d]: %s\n", error->type, error->message);
            if (error->filename) {
                fprintf(stderr, "  File: %s\n", error->filename);
            }
            if (error->line > 0) {
                fprintf(stderr, "  Line: %d, Col: %d\n", error->line, error->column);
            }
        } else {
            fprintf(stderr, "[DEBUG] No errors detected\n");
        }

        /* Print result (for debugging) */
        /* Uncomment to see result:
        if (result.type != FL_TYPE_NULL) {
            printf("Result: ");
            fl_value_print(result);
            printf("\n");
        }
        */

        /* Cleanup */
        fl_runtime_destroy(runtime);
        ast_free_all();

        return 0;
    }

    /* ========================================
       REPL COMMAND: Interactive shell
       ======================================== */
    else if (strcmp(cmd, "repl") == 0) {
        printf("FreeLang v1.0.0 REPL\n");
        printf("Type 'exit' or 'quit' to quit\n\n");

        /* Initialize AST pool */
        ast_init_pool(10000);

        /* Create runtime */
        fl_runtime_t *runtime = fl_runtime_create();
        if (!runtime) {
            fprintf(stderr, "Error: Runtime creation failed\n");
            ast_free_all();
            return 1;
        }

        char input[4096];
        int line_num = 1;

        while (1) {
            printf("fl> ");
            fflush(stdout);

            /* Read input line */
            if (!fgets(input, sizeof(input), stdin)) {
                break;
            }

            /* Check exit commands */
            if (strcmp(input, "exit\n") == 0 || strcmp(input, "quit\n") == 0) {
                break;
            }

            /* Skip empty lines */
            if (input[0] == '\n') {
                continue;
            }

            /* Execute input */
            fl_value_t result = fl_runtime_eval(runtime, input);

            /* Check for errors */
            fl_error_t *error = fl_runtime_get_error(runtime);
            if (error) {
                fprintf(stderr, "Error [%d]: %s\n", error->type, error->message);
            }

            /* Print result */
            if (result.type != FL_TYPE_NULL) {
                printf("=> ");
                fl_value_print(result);
                printf("\n");
            }

            line_num++;
        }

        /* Cleanup */
        fl_runtime_destroy(runtime);
        ast_free_all();

        printf("\nGoodbye!\n");
        return 0;
    }

    /* ========================================
       TEST COMMAND: Run AST tests
       ======================================== */
    else if (strcmp(cmd, "test") == 0) {
        printf("Running AST tests...\n");
        ast_init_pool(10000);

        printf("AST pool initialized\n");
        ast_pool_stats();

        ast_free_all();
        printf("Tests complete\n");
        return 0;
    }

    /* ========================================
       UNKNOWN COMMAND
       ======================================== */
    else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        fprintf(stderr, "Use 'run', 'repl', or 'test'\n");
        return 1;
    }

    return 0;
}
