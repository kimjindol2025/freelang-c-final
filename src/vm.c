/**
 * FreeLang Virtual Machine Implementation
 * Stack-based bytecode executor (1,200+ lines)
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "../include/vm.h"
#include "../include/compiler.h"
#include "../include/runtime.h"
#include "../include/closure.h"
#include "../include/stdlib_fl.h"
#include "../include/compression.h"
#include "../include/http_secure.h"
#include "../include/process.h"
#include "../include/cluster.h"
#include "../include/introspect.h"
#include "../include/logger.h"
#include "../include/autodoc.h"
#include <sys/wait.h>

/* ============================================================
   VM Global State
   ============================================================ */

/* Simple global variable storage */
typedef struct {
    char *key;
    fl_value_t value;
} GlobalVar;

static GlobalVar *vm_globals = NULL;
static size_t vm_globals_count = 0;
static size_t vm_globals_capacity = 0;

/* Bytecode interpreter state during execution */
typedef struct {
    const uint8_t *code;
    size_t ip;              /* Instruction pointer */
    const Chunk *chunk;     /* Reference to constants */
} BytecodeState;

/* ============================================================
   Helper Functions
   ============================================================ */

static void ensure_globals_capacity(void) {
    if (vm_globals_count >= vm_globals_capacity) {
        vm_globals_capacity = (vm_globals_capacity == 0) ? 32 : vm_globals_capacity * 2;
        vm_globals = (GlobalVar*)realloc(vm_globals,
            sizeof(GlobalVar) * vm_globals_capacity);
    }
}

static void set_global_var(const char *name, fl_value_t value) {
    /* Find existing variable */
    for (size_t i = 0; i < vm_globals_count; i++) {
        if (strcmp(vm_globals[i].key, name) == 0) {
            vm_globals[i].value = value;
            return;
        }
    }

    /* Add new variable */
    ensure_globals_capacity();
    vm_globals[vm_globals_count].key = (char*)malloc(strlen(name) + 1);
    strcpy(vm_globals[vm_globals_count].key, name);
    vm_globals[vm_globals_count].value = value;
    vm_globals_count++;
}

static fl_value_t get_global_var(const char *name) {
    for (size_t i = 0; i < vm_globals_count; i++) {
        if (strcmp(vm_globals[i].key, name) == 0) {
            return vm_globals[i].value;
        }
    }
    fl_value_t null_val;
    null_val.type = FL_TYPE_NULL;
    return null_val;
}

/* Read 4-byte address from bytecode */
static uint32_t read_addr(const uint8_t *code, size_t *ip) {
    uint32_t val = 0;
    val |= (uint32_t)code[(*ip)++] << 24;
    val |= (uint32_t)code[(*ip)++] << 16;
    val |= (uint32_t)code[(*ip)++] << 8;
    val |= (uint32_t)code[(*ip)++];
    return val;
}

/* Check if two values are equal */
static int values_equal(fl_value_t a, fl_value_t b) {
    if (a.type != b.type) return 0;

    switch (a.type) {
        case FL_TYPE_NULL:
            return 1;
        case FL_TYPE_BOOL:
            return a.data.bool_val == b.data.bool_val;
        case FL_TYPE_INT:
            return a.data.int_val == b.data.int_val;
        case FL_TYPE_FLOAT:
            return fabs(a.data.float_val - b.data.float_val) < 1e-9;
        case FL_TYPE_STRING:
            return strcmp(a.data.string_val, b.data.string_val) == 0;
        default:
            return 0;
    }
}

/* Compare two numeric values */
static int values_less(fl_value_t a, fl_value_t b) {
    if (a.type == FL_TYPE_INT && b.type == FL_TYPE_INT) {
        return a.data.int_val < b.data.int_val;
    }
    if (a.type == FL_TYPE_FLOAT && b.type == FL_TYPE_FLOAT) {
        return a.data.float_val < b.data.float_val;
    }
    if (a.type == FL_TYPE_INT && b.type == FL_TYPE_FLOAT) {
        return (fl_float)a.data.int_val < b.data.float_val;
    }
    if (a.type == FL_TYPE_FLOAT && b.type == FL_TYPE_INT) {
        return a.data.float_val < (fl_float)b.data.int_val;
    }
    return 0;
}

/* Convert value to boolean */
static int value_to_bool(fl_value_t val) {
    switch (val.type) {
        case FL_TYPE_NULL:
            return 0;
        case FL_TYPE_BOOL:
            return val.data.bool_val;
        case FL_TYPE_INT:
            return val.data.int_val != 0;
        case FL_TYPE_FLOAT:
            return val.data.float_val != 0.0;
        case FL_TYPE_STRING:
            return strlen(val.data.string_val) > 0;
        default:
            return 1;
    }
}

/* ============================================================
   VM Creation/Destruction
   ============================================================ */

fl_vm_t* fl_vm_create(void) {
    fl_vm_t* vm = (fl_vm_t*)malloc(sizeof(fl_vm_t));
    if (!vm) return NULL;

    vm->stack_top = 0;
    vm->frame_count = 0;
    vm->local_count = 0;
    memset(vm->stack, 0, sizeof(vm->stack));
    memset(vm->frames, 0, sizeof(vm->frames));
    memset(vm->locals, 0, sizeof(vm->locals));

    /* Initialize globals */
    vm->globals.type = FL_TYPE_OBJECT;
    vm->globals.data.object_val = NULL;

    vm->gc = NULL;

    /* Initialize exception handling */
    vm->exception_active = false;
    vm->exception_handler = 0;
    vm->exception.type = FL_TYPE_NULL;

    /* Initialize closure context */
    vm->current_closure = NULL;

    return vm;
}

void fl_vm_destroy(fl_vm_t* vm) {
    if (!vm) return;

    /* Free global variables */
    for (size_t i = 0; i < vm_globals_count; i++) {
        free(vm_globals[i].key);
    }
    free(vm_globals);
    vm_globals = NULL;
    vm_globals_count = 0;
    vm_globals_capacity = 0;

    free(vm);
}

/* ============================================================
   Stack Operations
   ============================================================ */

void fl_vm_push(fl_vm_t* vm, fl_value_t value) {
    if (vm->stack_top < FL_VM_STACK_SIZE) {
        vm->stack[vm->stack_top++] = value;
    }
}

fl_value_t fl_vm_pop(fl_vm_t* vm) {
    if (vm->stack_top > 0) {
        return vm->stack[--vm->stack_top];
    }
    fl_value_t null_val;
    null_val.type = FL_TYPE_NULL;
    return null_val;
}

fl_value_t fl_vm_peek(fl_vm_t* vm) {
    if (vm->stack_top > 0) {
        return vm->stack[vm->stack_top - 1];
    }
    fl_value_t null_val;
    null_val.type = FL_TYPE_NULL;
    return null_val;
}

/* ============================================================
   Global Variable Management
   ============================================================ */

void fl_vm_set_global(fl_vm_t* vm, const char* name, fl_value_t value) {
    (void)vm;  /* Unused parameter */
    set_global_var(name, value);
}

fl_value_t fl_vm_get_global(fl_vm_t* vm, const char* name) {
    (void)vm;  /* Unused parameter */
    return get_global_var(name);
}

/* ============================================================
   Built-in Functions
   ============================================================ */

static fl_value_t builtin_println(fl_vm_t *vm, int argc) {
    if (argc > 0) {
        fl_value_t arg = fl_vm_pop(vm);
        switch (arg.type) {
            case FL_TYPE_NULL:
                printf("null\n");
                break;
            case FL_TYPE_BOOL:
                printf("%s\n", arg.data.bool_val ? "true" : "false");
                break;
            case FL_TYPE_INT:
                printf("%ld\n", arg.data.int_val);
                break;
            case FL_TYPE_FLOAT:
                printf("%f\n", arg.data.float_val);
                break;
            case FL_TYPE_STRING:
                printf("%s\n", arg.data.string_val);
                break;
            default:
                printf("<%p>\n", (void*)arg.data.string_val);
                break;
        }
    } else {
        printf("\n");
    }

    fl_value_t ret;
    ret.type = FL_TYPE_NULL;
    return ret;
}

static fl_value_t builtin_print(fl_vm_t *vm, int argc) {
    if (argc > 0) {
        fl_value_t arg = fl_vm_pop(vm);
        switch (arg.type) {
            case FL_TYPE_NULL:
                printf("null");
                break;
            case FL_TYPE_BOOL:
                printf("%s", arg.data.bool_val ? "true" : "false");
                break;
            case FL_TYPE_INT:
                printf("%ld", arg.data.int_val);
                break;
            case FL_TYPE_FLOAT:
                printf("%f", arg.data.float_val);
                break;
            case FL_TYPE_STRING:
                printf("%s", arg.data.string_val);
                break;
            default:
                printf("<%p>", (void*)arg.data.string_val);
                break;
        }
    }

    fl_value_t ret;
    ret.type = FL_TYPE_NULL;
    return ret;
}

static void call_builtin(fl_vm_t *vm, const char *name, int argc) {
    if (strcmp(name, "println") == 0) {
        fl_value_t ret = builtin_println(vm, argc);
        fl_vm_push(vm, ret);
    } else if (strcmp(name, "print") == 0) {
        fl_value_t ret = builtin_print(vm, argc);
        fl_vm_push(vm, ret);
    } else if (strcmp(name, "write_bytes_file") == 0) {
        /* write_bytes_file(filename, byte_array) */
        fl_value_t* args = malloc(argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) {
            args[argc - 1 - i] = fl_vm_pop(vm);
        }
        fl_value_t ret = fl_write_bytes_file(args, argc);
        fl_vm_push(vm, ret);
        free(args);
    } else if (strcmp(name, "read_file") == 0) {
        /* read_file(filename) */
        fl_value_t* args = malloc(argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) {
            args[argc - 1 - i] = fl_vm_pop(vm);
        }
        fl_value_t ret = fl_read_file(args, argc);
        fl_vm_push(vm, ret);
        free(args);
    } else if (strcmp(name, "len") == 0) {
        /* len(array/string) */
        fl_value_t* args = malloc(argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) {
            args[argc - 1 - i] = fl_vm_pop(vm);
        }
        fl_value_t ret = fl_len(args, argc);
        fl_vm_push(vm, ret);
        free(args);
    } else if (strcmp(name, "push") == 0) {
        /* push(array, value) */
        fl_value_t* args = malloc(argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) {
            args[argc - 1 - i] = fl_vm_pop(vm);
        }
        fl_value_t ret = fl_push(args, argc);
        fl_vm_push(vm, ret);
        free(args);
    } else if (strcmp(name, "bytes_new") == 0) {
        /* bytes_new(capacity) */
        fl_value_t* args = malloc(argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) {
            args[argc - 1 - i] = fl_vm_pop(vm);
        }
        fl_value_t ret = fl_bytes_new(args, argc);
        fl_vm_push(vm, ret);
        free(args);
    } else if (strcmp(name, "bytes_len") == 0) {
        /* bytes_len(bytes) */
        fl_value_t* args = malloc(argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) {
            args[argc - 1 - i] = fl_vm_pop(vm);
        }
        fl_value_t ret = fl_bytes_len(args, argc);
        fl_vm_push(vm, ret);
        free(args);
    } else if (strcmp(name, "bytes_set") == 0) {
        /* bytes_set(bytes, idx, value) */
        fl_value_t* args = malloc(argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) {
            args[argc - 1 - i] = fl_vm_pop(vm);
        }
        fl_value_t ret = fl_bytes_set(args, argc);
        fl_vm_push(vm, ret);
        free(args);
    } else if (strcmp(name, "bytes_get") == 0) {
        /* bytes_get(bytes, idx) */
        fl_value_t* args = malloc(argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) {
            args[argc - 1 - i] = fl_vm_pop(vm);
        }
        fl_value_t ret = fl_bytes_get(args, argc);
        fl_vm_push(vm, ret);
        free(args);
    } else if (strcmp(name, "bytes_write_u32") == 0) {
        /* bytes_write_u32(bytes, offset, value) */
        fl_value_t* args = malloc(argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) {
            args[argc - 1 - i] = fl_vm_pop(vm);
        }
        fl_value_t ret = fl_bytes_write_u32(args, argc);
        fl_vm_push(vm, ret);
        free(args);
    } else if (strcmp(name, "bytes_write_u64") == 0) {
        /* bytes_write_u64(bytes, offset, value) */
        fl_value_t* args = malloc(argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) {
            args[argc - 1 - i] = fl_vm_pop(vm);
        }
        fl_value_t ret = fl_bytes_write_u64(args, argc);
        fl_vm_push(vm, ret);
        free(args);
    /* ── HTTP Secure-Pipeline (helmet 대체) ── */
    } else if (strcmp(name, "http_secure_headers") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) {
            args[argc - 1 - i] = fl_vm_pop(vm);
        }
        fl_value_t ret = fl_http_secure_headers(args, argc);
        fl_vm_push(vm, ret);
        free(args);
    } else if (strcmp(name, "http_csp") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) {
            args[argc - 1 - i] = fl_vm_pop(vm);
        }
        fl_value_t ret = fl_http_csp(args, argc);
        fl_vm_push(vm, ret);
        free(args);
    } else if (strcmp(name, "http_hsts") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) {
            args[argc - 1 - i] = fl_vm_pop(vm);
        }
        fl_value_t ret = fl_http_hsts(args, argc);
        fl_vm_push(vm, ret);
        free(args);
    } else if (strcmp(name, "http_response") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) {
            args[argc - 1 - i] = fl_vm_pop(vm);
        }
        fl_value_t ret = fl_http_response(args, argc);
        fl_vm_push(vm, ret);
        free(args);
    } else if (strcmp(name, "http_response_json") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) {
            args[argc - 1 - i] = fl_vm_pop(vm);
        }
        fl_value_t ret = fl_http_response_json(args, argc);
        fl_vm_push(vm, ret);
        free(args);
    /* ── Type Conversion ── */
    } else if (strcmp(name, "str") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_string_convert(args, argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "int") == 0 || strcmp(name, "num") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_number_convert(args, argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "bool") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_bool_convert(args, argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "typeof") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_typeof(args, argc);
        fl_vm_push(vm, ret); free(args);
    /* ── Crypto (Phase 5: bcrypt 대체 — SHA-256/HMAC/PBKDF2/CSPRNG) ── */
    } else if (strcmp(name, "sha256") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_sha256(args, argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "hmac_sha256") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_hmac_sha256(args, argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "pbkdf2") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_pbkdf2_hmac_sha256(args, argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "crypto_random") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_crypto_random(args, argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "bytes_to_hex") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_bytes_to_hex(args, argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "crypto_compare") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_crypto_compare(args, argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "u32_rotr") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_u32_rotr(args, argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "u32_add") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_u32_add(args, argc);
        fl_vm_push(vm, ret); free(args);

    /* ===== Phase 8: MOSS-Kernel-Runner 빌트인 함수 ===== */
    /* str(val), number(val), bool(val) 형 변환 */
    } else if (strcmp(name, "str") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_string_convert(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "number") == 0 || strcmp(name, "int") == 0 ||
               strcmp(name, "float") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_number_convert(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "typeof") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_typeof(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "keys") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_object_keys(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "values") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_object_values(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "upper") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_string_upper(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "lower") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_string_lower(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "system_metrics") == 0 ||
               strcmp(name, "System.metrics") == 0) {
        for (int i = 0; i < argc; i++) fl_vm_pop(vm);
        fl_value_t ret = fl_system_metrics(NULL, 0);
        fl_vm_push(vm, ret);
    } else if (strcmp(name, "process_spawn") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_process_spawn(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "process_kill") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_process_kill(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "process_list") == 0) {
        fl_value_t* args = malloc(1 * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) fl_vm_pop(vm); /* 인자 제거 */
        fl_value_t ret = fl_process_list(args, 0);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "process_restart") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_process_restart(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "process_pid") == 0) {
        for (int i = 0; i < argc; i++) fl_vm_pop(vm);
        fl_value_t ret = fl_process_pid(NULL, 0);
        fl_vm_push(vm, ret);
    } else if (strcmp(name, "process_sleep") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_process_sleep(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "cluster_workers") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_cluster_workers(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    /* Phase 6: Vector-Vision builtin functions */
    } else if (strcmp(name, "vision_load") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_vision_load(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "vision_save") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_vision_save(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "vision_resize") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_vision_resize(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "vision_width") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_vision_width(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "vision_height") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_vision_height(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "vision_channels") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_vision_channels(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "vision_grayscale") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_vision_grayscale(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "vision_blur") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_vision_blur(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "vision_pixel_get") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_vision_pixel_get(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "vision_pixel_set") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_vision_pixel_set(args, (size_t)argc);
        fl_value_t null_ret; null_ret.type = FL_TYPE_NULL;
        fl_vm_push(vm, null_ret); free(args);
    } else if (strcmp(name, "vision_info") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_vision_info(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "vision_simd_caps") == 0) {
        for (int i = 0; i < argc; i++) fl_vm_pop(vm);
        fl_value_t ret = fl_vision_simd_caps(NULL, 0);
        fl_vm_push(vm, ret);
    /* Proof-Logger builtin functions */
    } else if (strcmp(name, "log_configure") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_log_configure_builtin(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "log_debug") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_log_debug_builtin(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "log_info") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_log_info_builtin(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "log_warn") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_log_warn_builtin(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "log_error") == 0) {
        fl_value_t* args = malloc((argc + 1) * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_value_t ret = fl_log_error_builtin(args, (size_t)argc);
        fl_vm_push(vm, ret); free(args);
    } else if (strcmp(name, "log_flush") == 0) {
        for (int i = 0; i < argc; i++) fl_vm_pop(vm);
        fl_value_t ret = fl_log_flush_builtin(NULL, 0);
        fl_vm_push(vm, ret);
    } else if (strcmp(name, "log_stats") == 0) {
        for (int i = 0; i < argc; i++) fl_vm_pop(vm);
        fl_value_t ret = fl_log_stats_builtin(NULL, 0);
        fl_vm_push(vm, ret);
    /* MOSS-Compressor v1.0 */
    } else if (strcmp(name, "compress") == 0) {
        fl_value_t* args = malloc((size_t)argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_vm_push(vm, fl_compress(args, (size_t)argc));
        free(args);
    } else if (strcmp(name, "decompress") == 0) {
        fl_value_t* args = malloc((size_t)argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_vm_push(vm, fl_decompress(args, (size_t)argc));
        free(args);
    } else if (strcmp(name, "gzip") == 0) {
        fl_value_t* args = malloc((size_t)argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_vm_push(vm, fl_gzip(args, (size_t)argc));
        free(args);
    } else if (strcmp(name, "gunzip") == 0) {
        fl_value_t* args = malloc((size_t)argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_vm_push(vm, fl_gunzip(args, (size_t)argc));
        free(args);
    } else if (strcmp(name, "compress_ratio") == 0) {
        fl_value_t* args = malloc((size_t)argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_vm_push(vm, fl_compress_ratio(args, (size_t)argc));
        free(args);
    } else if (strcmp(name, "compress_info") == 0) {
        fl_value_t* args = malloc((size_t)argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_vm_push(vm, fl_compress_info(args, (size_t)argc));
        free(args);
    /* Phase 8: MOSS-Autodoc - Self-Documenting API Engine */
    } else if (strcmp(name, "autodoc_init") == 0) {
        fl_value_t* args = malloc((size_t)argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_vm_push(vm, fl_autodoc_init_builtin(args, (size_t)argc));
        free(args);
    } else if (strcmp(name, "autodoc_register") == 0) {
        fl_value_t* args = malloc((size_t)argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_vm_push(vm, fl_autodoc_register_builtin(args, (size_t)argc));
        free(args);
    } else if (strcmp(name, "autodoc_add_param") == 0) {
        fl_value_t* args = malloc((size_t)argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_vm_push(vm, fl_autodoc_add_param_builtin(args, (size_t)argc));
        free(args);
    } else if (strcmp(name, "autodoc_json") == 0) {
        fl_value_t* args = malloc((size_t)argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_vm_push(vm, fl_autodoc_json_builtin(args, (size_t)argc));
        free(args);
    } else if (strcmp(name, "autodoc_html") == 0) {
        fl_value_t* args = malloc((size_t)argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_vm_push(vm, fl_autodoc_html_builtin(args, (size_t)argc));
        free(args);
    } else if (strcmp(name, "autodoc_routes_json") == 0) {
        fl_value_t* args = malloc((size_t)argc * sizeof(fl_value_t));
        for (int i = 0; i < argc; i++) args[argc - 1 - i] = fl_vm_pop(vm);
        fl_vm_push(vm, fl_autodoc_routes_json_builtin(args, (size_t)argc));
        free(args);
    } else if (strcmp(name, "autodoc_count") == 0) {
        fl_value_t null_args; null_args.type = FL_TYPE_NULL;
        fl_vm_push(vm, fl_autodoc_count_builtin(&null_args, 0));
    } else {
        /* Unknown builtin - push null */
        fl_value_t ret;
        ret.type = FL_TYPE_NULL;
        fl_vm_push(vm, ret);
    }
}

/* ============================================================
   Function Calling
   ============================================================ */

fl_value_t fl_vm_call(fl_vm_t* vm, fl_value_t func, fl_value_t* args, size_t arg_count) {
    (void)func;
    (void)args;
    (void)arg_count;

    fl_value_t null_val;
    null_val.type = FL_TYPE_NULL;
    return null_val;
}

/* ============================================================
   Bytecode Execution - Main VM Loop
   ============================================================ */

fl_value_t fl_vm_execute(fl_vm_t* vm, const void* chunk_ptr) {
    const Chunk* chunk = (const Chunk*)chunk_ptr;
    if (!vm || !chunk || !chunk->code || chunk->code_len == 0) {
        fl_value_t ret;
        ret.type = FL_TYPE_NULL;
        return ret;
    }

    const uint8_t* bytecode = chunk->code;
    size_t size = chunk->code_len;
    size_t ip = 0;  /* Instruction pointer */
    fl_value_t result;
    result.type = FL_TYPE_NULL;

    /* Main instruction loop */
    int opcount = 0;
    while (ip < size) {
        opcount++;
        if (opcount > 1000000) {
            fprintf(stderr, "[VM] WARNING: too many opcodes (%d), breaking\n", opcount);
            break;
        }
        fl_opcode_t opcode = (fl_opcode_t)bytecode[ip++];

        switch (opcode) {
            /* ===== Stack Operations ===== */
            case FL_OP_PUSH_NULL: {
                fl_value_t val;
                val.type = FL_TYPE_NULL;
                fl_vm_push(vm, val);
                break;
            }

            case FL_OP_PUSH_BOOL: {
                uint32_t b = read_addr(bytecode, &ip);
                fl_value_t val;
                val.type = FL_TYPE_BOOL;
                val.data.bool_val = (b != 0);
                fl_vm_push(vm, val);
                break;
            }

            case FL_OP_PUSH_INT: {
                uint32_t idx = read_addr(bytecode, &ip);
                fl_value_t val;
                val.type = FL_TYPE_INT;

                /* Get int from constants pool */
                if (idx < (uint32_t)chunk->const_len && chunk->consts[idx].type == FL_TYPE_INT) {
                    val.data.int_val = chunk->consts[idx].data.int_val;
                } else {
                    /* Fallback if constant not found */
                    val.data.int_val = (fl_int)idx;
                }
                fl_vm_push(vm, val);
                break;
            }

            case FL_OP_PUSH_FLOAT: {
                uint32_t idx = read_addr(bytecode, &ip);
                fl_value_t val;
                val.type = FL_TYPE_FLOAT;

                /* Get float from constants pool */
                if (idx < (uint32_t)chunk->const_len && chunk->consts[idx].type == FL_TYPE_FLOAT) {
                    val.data.float_val = chunk->consts[idx].data.float_val;
                } else {
                    /* Fallback if constant not found */
                    val.data.float_val = (fl_float)idx;
                }
                fl_vm_push(vm, val);
                break;
            }

            case FL_OP_PUSH_STRING: {
                uint32_t idx = read_addr(bytecode, &ip);
                fl_value_t val;
                val.type = FL_TYPE_STRING;

                /* Get string from constants pool */
                if (idx < (uint32_t)chunk->const_len && chunk->consts[idx].type == FL_TYPE_STRING) {
                    /* Copy the string from constants */
                    const char* str = chunk->consts[idx].data.string_val;
                    val.data.string_val = (char*)malloc(strlen(str) + 1);
                    if (val.data.string_val) {
                        strcpy(val.data.string_val, str);
                    }
                } else {
                    /* Fallback if constant not found */
                    val.data.string_val = (char*)malloc(32);
                    snprintf(val.data.string_val, 32, "str_%u", idx);
                }
                fl_vm_push(vm, val);
                break;
            }

            case FL_OP_POP: {
                fl_vm_pop(vm);
                break;
            }

            case FL_OP_DUP: {
                fl_value_t val = fl_vm_peek(vm);
                fl_vm_push(vm, val);
                break;
            }

            /* ===== Arithmetic Operations ===== */
            case FL_OP_ADD: {
                fl_value_t b = fl_vm_pop(vm);
                fl_value_t a = fl_vm_pop(vm);
                fl_value_t result;

                if (a.type == FL_TYPE_INT && b.type == FL_TYPE_INT) {
                    result.type = FL_TYPE_INT;
                    result.data.int_val = a.data.int_val + b.data.int_val;
                } else if (a.type == FL_TYPE_STRING || b.type == FL_TYPE_STRING) {
                    /* String concatenation: 비-문자열 타입 자동 변환 */
                    char buf_a[64] = "null", buf_b[64] = "null";
                    const char *sa = buf_a, *sb = buf_b;
                    if (a.type == FL_TYPE_STRING) sa = a.data.string_val ? a.data.string_val : "null";
                    else if (a.type == FL_TYPE_INT)   snprintf(buf_a, sizeof(buf_a), "%ld", a.data.int_val);
                    else if (a.type == FL_TYPE_FLOAT)  snprintf(buf_a, sizeof(buf_a), "%g", a.data.float_val);
                    else if (a.type == FL_TYPE_BOOL)   snprintf(buf_a, sizeof(buf_a), "%s", a.data.bool_val ? "true" : "false");
                    if (b.type == FL_TYPE_STRING) sb = b.data.string_val ? b.data.string_val : "null";
                    else if (b.type == FL_TYPE_INT)   snprintf(buf_b, sizeof(buf_b), "%ld", b.data.int_val);
                    else if (b.type == FL_TYPE_FLOAT)  snprintf(buf_b, sizeof(buf_b), "%g", b.data.float_val);
                    else if (b.type == FL_TYPE_BOOL)   snprintf(buf_b, sizeof(buf_b), "%s", b.data.bool_val ? "true" : "false");
                    result.type = FL_TYPE_STRING;
                    size_t tlen = strlen(sa) + strlen(sb) + 1;
                    result.data.string_val = (char*)malloc(tlen);
                    if (result.data.string_val) {
                        strcpy(result.data.string_val, sa);
                        strcat(result.data.string_val, sb);
                    }
                } else {
                    /* Numeric addition */
                    result.type = FL_TYPE_FLOAT;
                    fl_float av = (a.type == FL_TYPE_INT) ?
                        (fl_float)a.data.int_val : a.data.float_val;
                    fl_float bv = (b.type == FL_TYPE_INT) ?
                        (fl_float)b.data.int_val : b.data.float_val;
                    result.data.float_val = av + bv;
                }
                fl_vm_push(vm, result);
                break;
            }

            case FL_OP_SUB: {
                fl_value_t b = fl_vm_pop(vm);
                fl_value_t a = fl_vm_pop(vm);
                fl_value_t result;

                if (a.type == FL_TYPE_INT && b.type == FL_TYPE_INT) {
                    result.type = FL_TYPE_INT;
                    result.data.int_val = a.data.int_val - b.data.int_val;
                } else {
                    /* Numeric subtraction (float) */
                    result.type = FL_TYPE_FLOAT;
                    fl_float av = (a.type == FL_TYPE_INT) ?
                        (fl_float)a.data.int_val : a.data.float_val;
                    fl_float bv = (b.type == FL_TYPE_INT) ?
                        (fl_float)b.data.int_val : b.data.float_val;
                    result.data.float_val = av - bv;
                }
                fl_vm_push(vm, result);
                break;
            }

            case FL_OP_MUL: {
                fl_value_t b = fl_vm_pop(vm);
                fl_value_t a = fl_vm_pop(vm);
                fl_value_t result;

                if (a.type == FL_TYPE_INT && b.type == FL_TYPE_INT) {
                    result.type = FL_TYPE_INT;
                    result.data.int_val = a.data.int_val * b.data.int_val;
                } else {
                    /* Numeric multiplication (float) */
                    result.type = FL_TYPE_FLOAT;
                    fl_float av = (a.type == FL_TYPE_INT) ?
                        (fl_float)a.data.int_val : a.data.float_val;
                    fl_float bv = (b.type == FL_TYPE_INT) ?
                        (fl_float)b.data.int_val : b.data.float_val;
                    result.data.float_val = av * bv;
                }
                fl_vm_push(vm, result);
                break;
            }

            case FL_OP_DIV: {
                fl_value_t b = fl_vm_pop(vm);
                fl_value_t a = fl_vm_pop(vm);
                fl_value_t result;

                if (a.type == FL_TYPE_INT && b.type == FL_TYPE_INT) {
                    result.type = FL_TYPE_INT;
                    if (b.data.int_val != 0) {
                        result.data.int_val = a.data.int_val / b.data.int_val;
                    } else {
                        result.type = FL_TYPE_NULL;
                    }
                } else {
                    /* Numeric division (float) */
                    result.type = FL_TYPE_FLOAT;
                    fl_float av = (a.type == FL_TYPE_INT) ?
                        (fl_float)a.data.int_val : a.data.float_val;
                    fl_float bv = (b.type == FL_TYPE_INT) ?
                        (fl_float)b.data.int_val : b.data.float_val;

                    if (fabs(bv) < 1e-10) {
                        result.type = FL_TYPE_NULL;
                    } else {
                        result.data.float_val = av / bv;
                    }
                }
                fl_vm_push(vm, result);
                break;
            }

            case FL_OP_MOD: {
                fl_value_t b = fl_vm_pop(vm);
                fl_value_t a = fl_vm_pop(vm);
                fl_value_t result;
                result.type = FL_TYPE_INT;

                /* MOD only works with integers */
                if (a.type == FL_TYPE_INT && b.type == FL_TYPE_INT) {
                    if (b.data.int_val != 0) {
                        result.data.int_val = a.data.int_val % b.data.int_val;
                    } else {
                        result.type = FL_TYPE_NULL;
                    }
                } else {
                    /* Type error for float modulo */
                    result.type = FL_TYPE_NULL;
                }
                fl_vm_push(vm, result);
                break;
            }

            /* ===== Comparison Operations ===== */
            case FL_OP_EQ: {
                fl_value_t b = fl_vm_pop(vm);
                fl_value_t a = fl_vm_pop(vm);
                fl_value_t result;
                result.type = FL_TYPE_BOOL;
                result.data.bool_val = values_equal(a, b);
                fl_vm_push(vm, result);
                break;
            }

            case FL_OP_NEQ: {
                fl_value_t b = fl_vm_pop(vm);
                fl_value_t a = fl_vm_pop(vm);
                fl_value_t result;
                result.type = FL_TYPE_BOOL;
                result.data.bool_val = !values_equal(a, b);
                fl_vm_push(vm, result);
                break;
            }

            case FL_OP_LT: {
                fl_value_t b = fl_vm_pop(vm);
                fl_value_t a = fl_vm_pop(vm);
                fl_value_t result;
                result.type = FL_TYPE_BOOL;
                result.data.bool_val = values_less(a, b);
                fl_vm_push(vm, result);
                break;
            }

            case FL_OP_LTE: {
                fl_value_t b = fl_vm_pop(vm);
                fl_value_t a = fl_vm_pop(vm);
                fl_value_t result;
                result.type = FL_TYPE_BOOL;
                result.data.bool_val = values_less(a, b) || values_equal(a, b);
                fl_vm_push(vm, result);
                break;
            }

            case FL_OP_GT: {
                fl_value_t b = fl_vm_pop(vm);
                fl_value_t a = fl_vm_pop(vm);
                fl_value_t result;
                result.type = FL_TYPE_BOOL;
                result.data.bool_val = values_less(b, a);
                fl_vm_push(vm, result);
                break;
            }

            case FL_OP_GTE: {
                fl_value_t b = fl_vm_pop(vm);
                fl_value_t a = fl_vm_pop(vm);
                fl_value_t result;
                result.type = FL_TYPE_BOOL;
                result.data.bool_val = values_less(b, a) || values_equal(a, b);
                fl_vm_push(vm, result);
                break;
            }

            /* ===== Logic Operations ===== */
            case FL_OP_AND: {
                fl_value_t b = fl_vm_pop(vm);
                fl_value_t a = fl_vm_pop(vm);
                fl_value_t result;
                result.type = FL_TYPE_BOOL;
                result.data.bool_val = value_to_bool(a) && value_to_bool(b);
                fl_vm_push(vm, result);
                break;
            }

            case FL_OP_OR: {
                fl_value_t b = fl_vm_pop(vm);
                fl_value_t a = fl_vm_pop(vm);
                fl_value_t result;
                result.type = FL_TYPE_BOOL;
                result.data.bool_val = value_to_bool(a) || value_to_bool(b);
                fl_vm_push(vm, result);
                break;
            }

            case FL_OP_NOT: {
                fl_value_t a = fl_vm_pop(vm);
                fl_value_t result;
                result.type = FL_TYPE_BOOL;
                result.data.bool_val = !value_to_bool(a);
                fl_vm_push(vm, result);
                break;
            }

            /* ===== Control Flow ===== */
            case FL_OP_JMP: {
                uint32_t addr = read_addr(bytecode, &ip);
                ip = (size_t)addr;
                break;
            }

            case FL_OP_JMPT: {
                uint32_t addr = read_addr(bytecode, &ip);
                fl_value_t cond = fl_vm_pop(vm);
                if (value_to_bool(cond)) {
                    ip = (size_t)addr;
                }
                break;
            }

            case FL_OP_JMPF: {
                uint32_t addr = read_addr(bytecode, &ip);
                fl_value_t cond = fl_vm_pop(vm);
                if (!value_to_bool(cond)) {
                    ip = (size_t)addr;
                }
                break;
            }

            case FL_OP_CALL: {
                uint32_t argc = read_addr(bytecode, &ip);

                /* Pop function reference */
                fl_value_t func = fl_vm_pop(vm);

                /* Handle closure calls */
                if (func.type == FL_TYPE_CLOSURE) {
                    fl_closure_t* closure = func.data.closure_val;

                    if (!closure || !closure->func) {
                        fprintf(stderr, "[VM] ERROR: Invalid closure\n");
                        fl_value_t null_val;
                        null_val.type = FL_TYPE_NULL;
                        fl_vm_push(vm, null_val);
                        break;
                    }

                    /* Check arity */
                    if (argc != closure->func->arity) {
                        fprintf(stderr, "[VM] ERROR: Closure expects %zu arguments, got %zu\n",
                                closure->func->arity, argc);
                        fl_value_t null_val;
                        null_val.type = FL_TYPE_NULL;
                        fl_vm_push(vm, null_val);
                        break;
                    }

                    /* Pop arguments from stack */
                    fl_value_t args[256];
                    for (int i = (int)argc - 1; i >= 0; i--) {
                        args[i] = fl_vm_pop(vm);
                    }

                    /* Save current context */
                    fl_value_t saved_locals[256];
                    int saved_local_count = vm->local_count;
                    fl_closure_t* saved_closure = vm->current_closure;

                    for (int i = 0; i < vm->local_count; i++) {
                        saved_locals[i] = vm->locals[i];
                    }

                    /* Setup closure context */
                    vm->current_closure = closure;
                    vm->local_count = (int)argc;
                    for (int i = 0; i < (int)argc; i++) {
                        vm->locals[i] = args[i];
                    }

                    /* Create temporary chunk for closure bytecode */
                    Chunk temp_chunk;
                    temp_chunk.code = closure->func->bytecode;
                    temp_chunk.code_len = closure->func->bytecode_size;
                    temp_chunk.code_capacity = closure->func->bytecode_size;
                    temp_chunk.consts = closure->func->consts;
                    temp_chunk.const_len = closure->func->const_count;
                    temp_chunk.locals_count = (int)argc;
                    temp_chunk.line_map = NULL;
                    temp_chunk.line_map_capacity = 0;

                    /* Execute closure bytecode */
                    fl_value_t result = fl_vm_execute(vm, (const void*)&temp_chunk);

                    /* Restore caller's context */
                    vm->local_count = saved_local_count;
                    vm->current_closure = saved_closure;
                    for (int i = 0; i < saved_local_count; i++) {
                        vm->locals[i] = saved_locals[i];
                    }

                    /* Push return value */
                    fl_vm_push(vm, result);
                    break;
                }

                /* For built-in functions passed as strings */
                if (func.type == FL_TYPE_STRING) {
                    const char* func_name = func.data.string_val;

                    fprintf(stderr, "[VM] CALL %s (runtime=%p)\n", func_name, (void*)vm->runtime);

                    /* Check if it's a user-defined function */
                    if (vm->runtime) {
                        fl_runtime_t* runtime = (fl_runtime_t*)vm->runtime;
                        fl_function_t* user_func = fl_runtime_find_function(runtime, func_name);
                        fprintf(stderr, "[VM] Found user function: %p\n", (void*)user_func);
                        fflush(stderr);

                        if (user_func) {
                            fprintf(stderr, "[VM] user_func=%p, is_native=%d\n",
                                    (void*)user_func, user_func->is_native);
                            fflush(stderr);
                        }

                        if (user_func && !user_func->is_native) {
                            /* User-defined function - execute its bytecode */
                            fprintf(stderr, "[VM] ✅ Executing user function: %s with %d args\n",
                                    func_name, argc);

                            /* Pop arguments from stack and save as locals */
                            fl_value_t args[256];
                            for (int i = (int)argc - 1; i >= 0; i--) {
                                args[i] = fl_vm_pop(vm);
                            }

                            /* Save current locals */
                            fl_value_t saved_locals[256];
                            int saved_local_count = vm->local_count;
                            for (int i = 0; i < vm->local_count; i++) {
                                saved_locals[i] = vm->locals[i];
                            }

                            /* Setup function locals with arguments */
                            vm->local_count = (int)argc;
                            for (int i = 0; i < (int)argc; i++) {
                                vm->locals[i] = args[i];
                            }

                            /* Create temporary chunk for function bytecode */
                            Chunk temp_chunk;
                            temp_chunk.code = user_func->bytecode;
                            temp_chunk.code_len = user_func->bytecode_size;
                            temp_chunk.code_capacity = user_func->bytecode_size;
                            temp_chunk.consts = user_func->consts;
                            temp_chunk.const_len = user_func->const_count;
                            temp_chunk.locals_count = (int)argc;
                            temp_chunk.line_map = NULL;
                            temp_chunk.line_map_capacity = 0;

                            /* Execute function bytecode */
                            fl_value_t result = fl_vm_execute(vm, (const void*)&temp_chunk);

                            /* Restore caller's locals */
                            vm->local_count = saved_local_count;
                            for (int i = 0; i < saved_local_count; i++) {
                                vm->locals[i] = saved_locals[i];
                            }

                            /* Push return value */
                            fl_vm_push(vm, result);
                            break;
                        }
                    }

                    /* Fall back to built-in function */
                    call_builtin(vm, func_name, (int)argc);
                }
                /* Handle FL_TYPE_FUNCTION calls (direct function pointers) */
                else if (func.type == FL_TYPE_FUNCTION) {
                    fl_function_t* func_ptr = func.data.func_val;

                    if (!func_ptr) {
                        fprintf(stderr, "[VM] ERROR: Invalid function pointer\n");
                        fl_value_t null_val;
                        null_val.type = FL_TYPE_NULL;
                        fl_vm_push(vm, null_val);
                        break;
                    }

                    if (func_ptr->is_native) {
                        fprintf(stderr, "[VM] WARNING: Native function call not yet implemented\n");
                        fl_value_t null_val;
                        null_val.type = FL_TYPE_NULL;
                        fl_vm_push(vm, null_val);
                    } else {
                        /* User-defined bytecode function */
                        if (argc != func_ptr->arity) {
                            fprintf(stderr, "[VM] ERROR: Function expects %zu arguments, got %zu\n",
                                    func_ptr->arity, argc);
                            fl_value_t null_val;
                            null_val.type = FL_TYPE_NULL;
                            fl_vm_push(vm, null_val);
                            break;
                        }

                        /* Pop arguments from stack */
                        fl_value_t args[256];
                        for (int i = (int)argc - 1; i >= 0; i--) {
                            args[i] = fl_vm_pop(vm);
                        }

                        /* Save current locals */
                        fl_value_t saved_locals[256];
                        int saved_local_count = vm->local_count;
                        for (int i = 0; i < vm->local_count; i++) {
                            saved_locals[i] = vm->locals[i];
                        }

                        /* Setup function locals with arguments */
                        vm->local_count = (int)argc;
                        for (int i = 0; i < (int)argc; i++) {
                            vm->locals[i] = args[i];
                        }

                        /* Create temporary chunk for function bytecode */
                        Chunk temp_chunk;
                        temp_chunk.code = func_ptr->bytecode;
                        temp_chunk.code_len = func_ptr->bytecode_size;
                        temp_chunk.code_capacity = func_ptr->bytecode_size;
                        temp_chunk.consts = func_ptr->consts;
                        temp_chunk.const_len = func_ptr->const_count;
                        temp_chunk.locals_count = (int)argc;
                        temp_chunk.line_map = NULL;
                        temp_chunk.line_map_capacity = 0;

                        /* Execute function bytecode */
                        fl_value_t result = fl_vm_execute(vm, (const void*)&temp_chunk);

                        /* Restore caller's locals */
                        vm->local_count = saved_local_count;
                        for (int i = 0; i < saved_local_count; i++) {
                            vm->locals[i] = saved_locals[i];
                        }

                        /* Push return value */
                        fl_vm_push(vm, result);
                    }
                }
                break;
            }

            case FL_OP_RET: {
                if (vm->stack_top > 0) {
                    result = fl_vm_pop(vm);
                }
                return result;
            }

            /* ===== Variable Operations ===== */
            case FL_OP_LOAD_LOCAL: {
                uint32_t idx = read_addr(bytecode, &ip);
                /* Load local variable from locals array */
                if (idx < (uint32_t)vm->local_count && idx < 256) {
                    fl_vm_push(vm, vm->locals[idx]);
                } else {
                    /* Out of bounds - push null */
                    fl_value_t null_val;
                    null_val.type = FL_TYPE_NULL;
                    fl_vm_push(vm, null_val);
                }
                break;
            }

            case FL_OP_STORE_LOCAL: {
                uint32_t idx = read_addr(bytecode, &ip);
                /* Store top of stack to local variable */
                fl_value_t val = fl_vm_pop(vm);
                if (idx < 256) {
                    vm->locals[idx] = val;
                    /* Update local_count if needed */
                    if (idx >= (uint32_t)vm->local_count) {
                        vm->local_count = idx + 1;
                    }
                }
                break;
            }

            case FL_OP_LOAD_GLOBAL: {
                uint32_t idx = read_addr(bytecode, &ip);
                char name[256];
                snprintf(name, sizeof(name), "var_%u", idx);
                fl_value_t val = get_global_var(name);
                fl_vm_push(vm, val);
                break;
            }

            case FL_OP_STORE_GLOBAL: {
                uint32_t idx = read_addr(bytecode, &ip);
                fl_value_t val = fl_vm_pop(vm);
                char name[256];
                snprintf(name, sizeof(name), "var_%u", idx);
                set_global_var(name, val);
                break;
            }

            /* ===== Array Operations ===== */
            case FL_OP_ARRAY_NEW: {
                uint32_t elem_count = read_addr(bytecode, &ip);
                fl_array_t *arr = (fl_array_t*)malloc(sizeof(fl_array_t));
                arr->capacity = (size_t)elem_count;
                arr->size = (size_t)elem_count;
                arr->elements = (fl_value_t*)malloc(sizeof(fl_value_t) * arr->capacity);

                /* Pop elements from stack and fill array (reverse order) */
                for (int i = (int)elem_count - 1; i >= 0; i--) {
                    arr->elements[i] = fl_vm_pop(vm);
                }

                fl_value_t val;
                val.type = FL_TYPE_ARRAY;
                val.data.array_val = arr;
                fl_vm_push(vm, val);
                break;
            }

            case FL_OP_ARRAY_GET: {
                fl_value_t idx = fl_vm_pop(vm);
                fl_value_t arr_val = fl_vm_pop(vm);
                fl_value_t result;
                result.type = FL_TYPE_NULL;

                if (arr_val.type == FL_TYPE_ARRAY && idx.type == FL_TYPE_INT) {
                    fl_array_t *arr = arr_val.data.array_val;
                    size_t i = (size_t)idx.data.int_val;
                    if (i < arr->size) {
                        result = arr->elements[i];
                    }
                } else if (arr_val.type == FL_TYPE_STRING && idx.type == FL_TYPE_INT) {
                    /* String indexing: return single character as string */
                    const char *str = arr_val.data.string_val;
                    size_t i = (size_t)idx.data.int_val;
                    if (i < strlen(str)) {
                        char ch[2] = {str[i], '\0'};
                        result.type = FL_TYPE_STRING;
                        result.data.string_val = (char*)malloc(2);
                        strcpy(result.data.string_val, ch);
                    }
                }
                fl_vm_push(vm, result);
                break;
            }

            case FL_OP_ARRAY_SET: {
                /* Pop: val (top), idx, arr (bottom) */
                fl_value_t val = fl_vm_pop(vm);
                fl_value_t idx = fl_vm_pop(vm);
                fl_value_t arr_val = fl_vm_pop(vm);

                if (arr_val.type == FL_TYPE_ARRAY && idx.type == FL_TYPE_INT) {
                    fl_array_t *arr = arr_val.data.array_val;
                    size_t i = (size_t)idx.data.int_val;
                    if (i < arr->capacity) {
                        arr->elements[i] = val;
                        if (i >= arr->size) {
                            arr->size = i + 1;
                        }
                    }
                }
                break;
            }

            case FL_OP_ARRAY_LEN: {
                fl_value_t val = fl_vm_pop(vm);
                fl_value_t result;
                result.type = FL_TYPE_INT;

                if (val.type == FL_TYPE_ARRAY) {
                    result.data.int_val = (fl_int)val.data.array_val->size;
                } else if (val.type == FL_TYPE_STRING) {
                    result.data.int_val = (fl_int)strlen(val.data.string_val);
                } else {
                    result.type = FL_TYPE_NULL;
                }
                fl_vm_push(vm, result);
                break;
            }

            /* ===== Object Operations ===== */
            case FL_OP_OBJECT_NEW: {
                fl_object_t *obj = (fl_object_t*)malloc(sizeof(fl_object_t));
                obj->capacity = 32;
                obj->size = 0;
                obj->keys = (char**)malloc(sizeof(char*) * obj->capacity);
                obj->values = (fl_value_t*)malloc(sizeof(fl_value_t) * obj->capacity);

                fl_value_t val;
                val.type = FL_TYPE_OBJECT;
                val.data.object_val = obj;
                fl_vm_push(vm, val);
                break;
            }

            case FL_OP_OBJECT_GET: {
                fl_value_t key_val = fl_vm_pop(vm);
                fl_value_t obj_val = fl_vm_pop(vm);

                fl_value_t get_result;
                get_result.type = FL_TYPE_NULL;  /* 기본값: null */

                if (obj_val.type == FL_TYPE_OBJECT && key_val.type == FL_TYPE_STRING) {
                    fl_object_t *obj = obj_val.data.object_val;
                    const char *key = key_val.data.string_val;

                    int found = 0;
                    if (obj) {
                        for (size_t i = 0; i < obj->size; i++) {
                            if (obj->keys[i] && strcmp(obj->keys[i], key) == 0) {
                                get_result = obj->values[i];
                                found = 1;
                                break;
                            }
                        }
                    }
                    (void)found;
                }
                fl_vm_push(vm, get_result);
                break;
            }

            case FL_OP_OBJECT_SET: {
                fl_value_t val = fl_vm_pop(vm);
                fl_value_t key_val = fl_vm_pop(vm);
                fl_value_t obj_val = fl_vm_pop(vm);

                if (obj_val.type == FL_TYPE_OBJECT && key_val.type == FL_TYPE_STRING) {
                    fl_object_t *obj = obj_val.data.object_val;
                    const char *key = key_val.data.string_val;

                    /* Find or add key */
                    for (size_t i = 0; i < obj->size; i++) {
                        if (strcmp(obj->keys[i], key) == 0) {
                            obj->values[i] = val;
                            goto obj_set_done;
                        }
                    }

                    /* Add new key */
                    if (obj->size < obj->capacity) {
                        obj->keys[obj->size] = (char*)malloc(strlen(key) + 1);
                        strcpy(obj->keys[obj->size], key);
                        obj->values[obj->size] = val;
                        obj->size++;
                    }
                }
                obj_set_done:
                break;
            }

            /* ===== Exception Handling ===== */
            case FL_OP_TRY_START: {
                /* Read catch handler address */
                uint32_t catch_addr = read_addr(bytecode, &ip);
                /* Save catch handler as instruction offset */
                vm->exception_handler = (size_t)catch_addr;
                break;
            }

            case FL_OP_CATCH_START: {
                /* Catch block started - exception value should be on stack */
                /* Read variable index for exception variable */
                uint32_t var_idx = read_addr(bytecode, &ip);
                if (vm->exception_active) {
                    /* Store exception in local variable */
                    if (var_idx < 256) {
                        vm->locals[var_idx] = vm->exception;
                    }
                    vm->exception_active = false;
                }
                break;
            }

            case FL_OP_THROW: {
                /* Pop exception value from stack */
                fl_value_t exception_val = fl_vm_pop(vm);
                vm->exception = exception_val;
                vm->exception_active = true;
                /* Jump to exception handler if set */
                if (vm->exception_handler > 0) {
                    ip = vm->exception_handler;
                } else {
                    /* No handler - return error */
                    fprintf(stderr, "[VM] Uncaught exception\n");
                    return exception_val;
                }
                break;
            }

            case FL_OP_CATCH_END: {
                /* Catch block ended - clear exception state */
                vm->exception_active = false;
                vm->exception_handler = 0;
                break;
            }

            /* ===== Closure Operations ===== */
            case FL_OP_MAKE_CLOSURE: {
                /* Read number of variables to capture */
                uint32_t captured_count = read_addr(bytecode, &ip);

                /* Pop function reference from stack */
                fl_value_t func_val = fl_vm_pop(vm);

                if (func_val.type == FL_TYPE_FUNCTION) {
                    fl_function_t* func = func_val.data.func_val;

                    /* Pop captured variables from stack (in reverse order) */
                    fl_captured_var_t* captured_vars = NULL;
                    if (captured_count > 0) {
                        captured_vars = (fl_captured_var_t*)malloc(captured_count * sizeof(fl_captured_var_t));
                        if (captured_vars) {
                            /* Pop from stack in reverse order */
                            for (int i = (int)captured_count - 1; i >= 0; i--) {
                                fl_value_t var_val = fl_vm_pop(vm);
                                /* TODO: Get variable name from compiler metadata */
                                /* For now, use dummy names */
                                char var_name[32];
                                snprintf(var_name, sizeof(var_name), "upval_%d", i);
                                captured_vars[i].name = (char*)malloc(strlen(var_name) + 1);
                                strcpy(captured_vars[i].name, var_name);
                                captured_vars[i].value = var_val;
                            }
                        }
                    }

                    /* Create closure */
                    fl_closure_t* closure = fl_closure_create(func, captured_vars, captured_count);

                    /* Free temporary captured vars array (fl_closure_create copies them) */
                    if (captured_vars) {
                        for (size_t i = 0; i < captured_count; i++) {
                            free(captured_vars[i].name);
                        }
                        free(captured_vars);
                    }

                    if (closure) {
                        /* Push closure value to stack */
                        fl_value_t closure_val = fl_value_from_closure(closure);
                        fl_vm_push(vm, closure_val);
                    } else {
                        /* Failed to create closure - push null */
                        fl_value_t null_val;
                        null_val.type = FL_TYPE_NULL;
                        fl_vm_push(vm, null_val);
                    }
                } else {
                    /* Error: trying to create closure from non-function */
                    fprintf(stderr, "[VM] ERROR: MAKE_CLOSURE requires function, got type %d\n", func_val.type);
                    fl_value_t null_val;
                    null_val.type = FL_TYPE_NULL;
                    fl_vm_push(vm, null_val);
                }
                break;
            }

            case FL_OP_LOAD_UPVALUE: {
                /* Read upvalue index */
                uint32_t upval_idx = read_addr(bytecode, &ip);

                /* Load from current closure's captured variables */
                if (vm->current_closure && upval_idx < vm->current_closure->captured_count) {
                    fl_vm_push(vm, vm->current_closure->captured_vars[upval_idx].value);
                } else {
                    /* Error: invalid upvalue access */
                    fprintf(stderr, "[VM] ERROR: LOAD_UPVALUE invalid index %u (closure=%p, count=%zu)\n",
                            upval_idx, (void*)vm->current_closure,
                            vm->current_closure ? vm->current_closure->captured_count : 0);
                    fl_value_t null_val;
                    null_val.type = FL_TYPE_NULL;
                    fl_vm_push(vm, null_val);
                }
                break;
            }

            case FL_OP_STORE_UPVALUE: {
                /* Read upvalue index */
                uint32_t upval_idx = read_addr(bytecode, &ip);

                /* Pop value from stack */
                fl_value_t val = fl_vm_pop(vm);

                /* Store in current closure's captured variables */
                if (vm->current_closure && upval_idx < vm->current_closure->captured_count) {
                    vm->current_closure->captured_vars[upval_idx].value = val;
                } else {
                    /* Error: invalid upvalue access */
                    fprintf(stderr, "[VM] ERROR: STORE_UPVALUE invalid index %u (closure=%p, count=%zu)\n",
                            upval_idx, (void*)vm->current_closure,
                            vm->current_closure ? vm->current_closure->captured_count : 0);
                }
                break;
            }

            /* ===== Process Management (Phase 8: MOSS-Kernel-Runner) ===== */
            case FL_OP_GET_METRICS: {
                /* System.metrics() → push object onto stack */
                fl_value_t none_args[1];
                fl_value_t metrics = fl_system_metrics(none_args, 0);
                fl_vm_push(vm, metrics);
                break;
            }

            case FL_OP_SPAWN_PROCESS: {
                /* stack: [name, script, autorestart] → [pid] */
                fl_value_t autorestart_v = fl_vm_pop(vm);
                fl_value_t script_v      = fl_vm_pop(vm);
                fl_value_t name_v        = fl_vm_pop(vm);
                fl_value_t spawn_args[3] = { name_v, script_v, autorestart_v };
                fl_value_t pid_v = fl_process_spawn(spawn_args, 3);
                fl_vm_push(vm, pid_v);
                break;
            }

            case FL_OP_KILL_PROCESS: {
                /* stack: [pid, signal] → [ok] */
                fl_value_t sig_v = fl_vm_pop(vm);
                fl_value_t pid_v = fl_vm_pop(vm);
                fl_value_t kill_args[2] = { pid_v, sig_v };
                fl_value_t ok_v = fl_process_kill(kill_args, 2);
                fl_vm_push(vm, ok_v);
                break;
            }

            case FL_OP_WAIT_PROCESS: {
                /* stack: [pid] → [exit_code] */
                fl_value_t pid_v = fl_vm_pop(vm);
                if (pid_v.type == FL_TYPE_INT) {
                    int status = 0;
                    waitpid((pid_t)pid_v.data.int_val, &status, 0);
                    int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                    fl_value_t ec;
                    ec.type = FL_TYPE_INT;
                    ec.data.int_val = code;
                    fl_vm_push(vm, ec);
                } else {
                    fl_value_t null_v; null_v.type = FL_TYPE_NULL;
                    fl_vm_push(vm, null_v);
                }
                break;
            }

            case FL_OP_CLUSTER_INIT: {
                /* stack: [n] → [] (spawns workers, master blocks) */
                fl_value_t n_v = fl_vm_pop(vm);
                fl_value_t cargs[1] = { n_v };
                fl_cluster_workers(cargs, 1);
                break;
            }

            case FL_OP_DAEMONIZE: {
                fl_value_t no_args[1];
                fl_system_daemonize(no_args, 0);
                break;
            }

            /* ===== Other ===== */
            case FL_OP_NOP:
                break;

            case FL_OP_HALT:
                fprintf(stderr, "[VM] HALT reached, returning\n");
                return result;

            default:
                fprintf(stderr, "Unknown opcode: %d\n", opcode);
                return result;
        }
    }

    return result;
}
