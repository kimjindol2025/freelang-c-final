/**
 * FreeLang Standard Library Functions
 * Core builtin functions available to all programs (100+ functions)
 */

#ifndef FL_STDLIB_H
#define FL_STDLIB_H

#include "freelang.h"

/* I/O Functions (5) */
fl_value_t fl_print(fl_value_t* args, size_t argc);
fl_value_t fl_println(fl_value_t* args, size_t argc);
fl_value_t fl_input(fl_value_t* args, size_t argc);
fl_value_t fl_write_bytes_file(fl_value_t* args, size_t argc);
fl_value_t fl_read_file(fl_value_t* args, size_t argc);

/* Core Array Functions */
fl_value_t fl_len(fl_value_t* args, size_t argc);
fl_value_t fl_push(fl_value_t* args, size_t argc);

/* String Functions (16) */
fl_value_t fl_string_length(fl_value_t* args, size_t argc);
fl_value_t fl_string_concat(fl_value_t* args, size_t argc);
fl_value_t fl_string_slice(fl_value_t* args, size_t argc);
fl_value_t fl_string_split(fl_value_t* args, size_t argc);
fl_value_t fl_string_trim(fl_value_t* args, size_t argc);
fl_value_t fl_string_upper(fl_value_t* args, size_t argc);
fl_value_t fl_string_lower(fl_value_t* args, size_t argc);
fl_value_t fl_string_contains(fl_value_t* args, size_t argc);
fl_value_t fl_string_starts_with(fl_value_t* args, size_t argc);
fl_value_t fl_string_ends_with(fl_value_t* args, size_t argc);
fl_value_t fl_string_replace(fl_value_t* args, size_t argc);
fl_value_t fl_string_index_of(fl_value_t* args, size_t argc);
fl_value_t fl_string_repeat(fl_value_t* args, size_t argc);
fl_value_t fl_string_pad_start(fl_value_t* args, size_t argc);
fl_value_t fl_string_pad_end(fl_value_t* args, size_t argc);
fl_value_t fl_string_char_at(fl_value_t* args, size_t argc);

/* Array Functions (17) */
fl_value_t fl_array_length(fl_value_t* args, size_t argc);
fl_value_t fl_array_push(fl_value_t* args, size_t argc);
fl_value_t fl_array_pop(fl_value_t* args, size_t argc);
fl_value_t fl_array_shift(fl_value_t* args, size_t argc);
fl_value_t fl_array_unshift(fl_value_t* args, size_t argc);
fl_value_t fl_array_map(fl_value_t* args, size_t argc);
fl_value_t fl_array_filter(fl_value_t* args, size_t argc);
fl_value_t fl_array_reduce(fl_value_t* args, size_t argc);
fl_value_t fl_array_join(fl_value_t* args, size_t argc);
fl_value_t fl_array_reverse(fl_value_t* args, size_t argc);
fl_value_t fl_array_sort(fl_value_t* args, size_t argc);
fl_value_t fl_array_includes(fl_value_t* args, size_t argc);
fl_value_t fl_array_slice(fl_value_t* args, size_t argc);
fl_value_t fl_array_index_of(fl_value_t* args, size_t argc);
fl_value_t fl_array_last_index_of(fl_value_t* args, size_t argc);

/* Math Functions (15) */
fl_value_t fl_math_abs(fl_value_t* args, size_t argc);
fl_value_t fl_math_sqrt(fl_value_t* args, size_t argc);
fl_value_t fl_math_pow(fl_value_t* args, size_t argc);
fl_value_t fl_math_floor(fl_value_t* args, size_t argc);
fl_value_t fl_math_ceil(fl_value_t* args, size_t argc);
fl_value_t fl_math_round(fl_value_t* args, size_t argc);
fl_value_t fl_math_min(fl_value_t* args, size_t argc);
fl_value_t fl_math_max(fl_value_t* args, size_t argc);
fl_value_t fl_math_random(fl_value_t* args, size_t argc);
fl_value_t fl_math_sin(fl_value_t* args, size_t argc);
fl_value_t fl_math_cos(fl_value_t* args, size_t argc);
fl_value_t fl_math_tan(fl_value_t* args, size_t argc);
fl_value_t fl_math_log(fl_value_t* args, size_t argc);
fl_value_t fl_math_exp(fl_value_t* args, size_t argc);

/* Type Conversion (6) */
fl_value_t fl_typeof(fl_value_t* args, size_t argc);
fl_value_t fl_string_convert(fl_value_t* args, size_t argc);
fl_value_t fl_number_convert(fl_value_t* args, size_t argc);
fl_value_t fl_bool_convert(fl_value_t* args, size_t argc);
fl_value_t fl_is_null(fl_value_t* args, size_t argc);
fl_value_t fl_array_from(fl_value_t* args, size_t argc);

/* Object Functions (4) */
fl_value_t fl_object_keys(fl_value_t* args, size_t argc);
fl_value_t fl_object_values(fl_value_t* args, size_t argc);
fl_value_t fl_object_entries(fl_value_t* args, size_t argc);
fl_value_t fl_object_assign(fl_value_t* args, size_t argc);

/* JSON Functions (2) */
fl_value_t fl_json_stringify(fl_value_t* args, size_t argc);
fl_value_t fl_json_parse(fl_value_t* args, size_t argc);

/* Bytes Functions (5) - Phase 3 */
fl_value_t fl_bytes_new(fl_value_t* args, size_t argc);
fl_value_t fl_bytes_len(fl_value_t* args, size_t argc);
fl_value_t fl_bytes_set(fl_value_t* args, size_t argc);
fl_value_t fl_bytes_get(fl_value_t* args, size_t argc);
fl_value_t fl_bytes_write_u32(fl_value_t* args, size_t argc);
fl_value_t fl_bytes_write_u64(fl_value_t* args, size_t argc);

/* Control Flow (2) */
fl_value_t fl_assert(fl_value_t* args, size_t argc);
fl_value_t fl_panic(fl_value_t* args, size_t argc);

/* Crypto Functions (Phase 5 - bcrypt 대체) */
/* SHA-256: sha256(bytes|string) -> bytes[32] */
fl_value_t fl_sha256(fl_value_t* args, size_t argc);
/* HMAC-SHA256: hmac_sha256(key_bytes, data_bytes) -> bytes[32] */
fl_value_t fl_hmac_sha256(fl_value_t* args, size_t argc);
/* PBKDF2: pbkdf2(password_bytes, salt_bytes, iterations, dklen) -> bytes */
fl_value_t fl_pbkdf2_hmac_sha256(fl_value_t* args, size_t argc);
/* Secure random: crypto_random(n) -> bytes[n] (CSPRNG via /dev/urandom) */
fl_value_t fl_crypto_random(fl_value_t* args, size_t argc);
/* bytes -> hex string: bytes_to_hex(bytes) -> string */
fl_value_t fl_bytes_to_hex(fl_value_t* args, size_t argc);
/* Timing-safe compare: crypto_compare(a_bytes, b_bytes) -> bool */
fl_value_t fl_crypto_compare(fl_value_t* args, size_t argc);
/* u32 bit rotation: u32_rotr(val, n) -> u32 */
fl_value_t fl_u32_rotr(fl_value_t* args, size_t argc);
/* u32 add with overflow (mod 2^32): u32_add(a, b) -> u32 */
fl_value_t fl_u32_add(fl_value_t* args, size_t argc);

/* HTTP Secure-Pipeline (5) - helmet 대체 네이티브 구현 */
fl_value_t fl_http_secure_headers(fl_value_t* args, size_t argc);
fl_value_t fl_http_csp(fl_value_t* args, size_t argc);
fl_value_t fl_http_hsts(fl_value_t* args, size_t argc);
fl_value_t fl_http_response(fl_value_t* args, size_t argc);
fl_value_t fl_http_response_json(fl_value_t* args, size_t argc);

/* Register all stdlib functions */
void fl_stdlib_register(fl_runtime_t* runtime);

#endif /* FL_STDLIB_H */
