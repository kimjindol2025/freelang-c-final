/**
 * FreeLang Compression Engine
 * DEFLATE (RFC 1951) + GZIP (RFC 1952) - Pure C
 * SIMD Layer: AVX2 (#ifdef __AVX2__) / NEON (#ifdef __ARM_NEON)
 * Phase: MOSS-Compressor v1.0
 */

#ifndef FL_COMPRESSION_H
#define FL_COMPRESSION_H

#include "freelang.h"

/* Compression levels */
#define FL_COMPRESS_NONE    0
#define FL_COMPRESS_FAST    1
#define FL_COMPRESS_DEFAULT 6
#define FL_COMPRESS_BEST    9

/* LZ77 parameters (RFC 1951) */
#define FL_DEFLATE_WIN_SIZE  32768
#define FL_DEFLATE_MIN_MATCH 3
#define FL_DEFLATE_MAX_MATCH 258
#define FL_DEFLATE_HASH_BITS 15
#define FL_DEFLATE_HASH_SIZE (1 << FL_DEFLATE_HASH_BITS)

/**
 * Bit Stream - bit_pack 인스트럭션 기반 Zero-Copy 버퍼
 * 비트 단위 누적 후 바이트 플러시. 동적 확장, 외부 memcpy 없음.
 */
typedef struct fl_bit_stream {
    uint8_t* buf;
    size_t   cap;
    size_t   byte_pos;
    uint32_t bit_cache;      /* 최대 32비트 비트 누적 캐시 */
    int      bits_in_cache;
} fl_bit_stream_t;

/* Bit stream API */
void fl_bit_stream_init(fl_bit_stream_t* bs, size_t initial_cap);
void fl_bit_stream_write(fl_bit_stream_t* bs, uint32_t val, int bits);
void fl_bit_stream_flush(fl_bit_stream_t* bs);
void fl_bit_stream_free(fl_bit_stream_t* bs);

/* Core compression/decompression */
fl_bytes_t* fl_deflate_compress(const uint8_t* src, size_t src_len, int level);
fl_bytes_t* fl_deflate_decompress(const uint8_t* src, size_t src_len);
fl_bytes_t* fl_gzip_wrap(const uint8_t* src, size_t src_len, int level);
fl_bytes_t* fl_gzip_unwrap(const uint8_t* src, size_t src_len);

/* FreeLang stdlib bindings */
fl_value_t fl_compress(fl_value_t* args, size_t argc);
fl_value_t fl_decompress(fl_value_t* args, size_t argc);
fl_value_t fl_gzip(fl_value_t* args, size_t argc);
fl_value_t fl_gunzip(fl_value_t* args, size_t argc);
fl_value_t fl_compress_ratio(fl_value_t* args, size_t argc);
fl_value_t fl_compress_info(fl_value_t* args, size_t argc);

#endif /* FL_COMPRESSION_H */
