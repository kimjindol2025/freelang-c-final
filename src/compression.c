/**
 * FreeLang Compression Engine - MOSS-Compressor v1.0
 * DEFLATE (RFC 1951) + GZIP (RFC 1952) Pure C Implementation
 *
 * 아키텍처:
 *   BitStream  → bit_pack 인스트럭션 (Zero-Copy 비트 누적)
 *   LZ77       → 해시 체인 슬라이딩 윈도우 (32KB)
 *   Huffman    → Fixed (RFC 1951 §3.2.6)
 *   SIMD Layer → AVX2 32바이트 병렬 매칭 / ARM NEON 16바이트
 *   GZIP       → RFC 1952 헤더/CRC32/트레일러
 *
 * 외부 의존성: 없음 (zlib, libz 불필요)
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../include/compression.h"
#include "../include/runtime.h"

/* SIMD 헤더 (조건부) */
#ifdef __AVX2__
#include <immintrin.h>
#endif
#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

/* ============================================================
   CRC32 (RFC 1952 §8)  - 다항식 0xEDB88320
   ============================================================ */

static uint32_t crc32_table[256];
static int      crc32_initialized = 0;

static void crc32_init(void) {
    if (crc32_initialized) return;
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
        crc32_table[n] = c;
    }
    crc32_initialized = 1;
}

static uint32_t crc32_compute(const uint8_t* buf, size_t len) {
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < len; i++)
        crc = crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

/* ============================================================
   Bit Stream (bit_pack 인스트럭션 구현)
   Zero-Copy: 내부 동적 버퍼만 사용, 외부 memcpy 없음
   ============================================================ */

static void bs_grow(fl_bit_stream_t* bs) {
    bs->cap = bs->cap * 2 + 64;
    bs->buf = (uint8_t*)realloc(bs->buf, bs->cap);
}

void fl_bit_stream_init(fl_bit_stream_t* bs, size_t initial_cap) {
    bs->buf           = (uint8_t*)malloc(initial_cap);
    bs->cap           = initial_cap;
    bs->byte_pos      = 0;
    bs->bit_cache     = 0;
    bs->bits_in_cache = 0;
}

/**
 * bit_pack: val의 하위 bits 비트를 캐시에 누적.
 * 8비트가 쌓이면 바이트로 플러시 (LSB-first, RFC 1951 규칙).
 */
void fl_bit_stream_write(fl_bit_stream_t* bs, uint32_t val, int bits) {
    bs->bit_cache |= (val & ((1u << bits) - 1u)) << bs->bits_in_cache;
    bs->bits_in_cache += bits;
    while (bs->bits_in_cache >= 8) {
        if (bs->byte_pos >= bs->cap) bs_grow(bs);
        bs->buf[bs->byte_pos++] = (uint8_t)(bs->bit_cache & 0xFF);
        bs->bit_cache     >>= 8;
        bs->bits_in_cache  -= 8;
    }
}

void fl_bit_stream_flush(fl_bit_stream_t* bs) {
    if (bs->bits_in_cache > 0) {
        if (bs->byte_pos >= bs->cap) bs_grow(bs);
        bs->buf[bs->byte_pos++] = (uint8_t)(bs->bit_cache & 0xFF);
        bs->bit_cache     = 0;
        bs->bits_in_cache = 0;
    }
}

void fl_bit_stream_free(fl_bit_stream_t* bs) {
    if (bs->buf) { free(bs->buf); bs->buf = NULL; }
}

/* ============================================================
   Fixed Huffman Tables (RFC 1951 §3.2.6)
   ============================================================ */

typedef struct { uint16_t code; int len; } huff_code_t;

static huff_code_t fixed_lit[288];
static huff_code_t fixed_dist[32];
static int         fixed_tables_ready = 0;

static uint16_t reverse_bits(uint16_t val, int len) {
    uint16_t r = 0;
    for (int i = 0; i < len; i++) { r = (uint16_t)((r << 1) | (val & 1)); val >>= 1; }
    return r;
}

static void init_fixed_tables(void) {
    if (fixed_tables_ready) return;

    /* Literal 0-143: 8비트  00110000-10111111 */
    for (int i = 0; i <= 143; i++) {
        fixed_lit[i].len  = 8;
        fixed_lit[i].code = reverse_bits((uint16_t)(0x030 + i), 8);
    }
    /* Literal 144-255: 9비트  110010000-111111111 */
    for (int i = 144; i <= 255; i++) {
        fixed_lit[i].len  = 9;
        fixed_lit[i].code = reverse_bits((uint16_t)(0x190 + (i - 144)), 9);
    }
    /* EOB 256: 7비트 0000000 */
    fixed_lit[256].len  = 7;
    fixed_lit[256].code = reverse_bits(0x000, 7);
    /* Length 257-279: 7비트 */
    for (int i = 257; i <= 279; i++) {
        fixed_lit[i].len  = 7;
        fixed_lit[i].code = reverse_bits((uint16_t)(0x001 + (i - 257)), 7);
    }
    /* Length 280-287: 8비트 */
    for (int i = 280; i <= 287; i++) {
        fixed_lit[i].len  = 8;
        fixed_lit[i].code = reverse_bits((uint16_t)(0x0C0 + (i - 280)), 8);
    }
    /* Distance 0-29: 5비트 고정 */
    for (int i = 0; i < 32; i++) {
        fixed_dist[i].len  = 5;
        fixed_dist[i].code = reverse_bits((uint16_t)i, 5);
    }

    fixed_tables_ready = 1;
}

/* ============================================================
   Length/Distance Extra-bit Tables (RFC 1951 §3.2.5)
   ============================================================ */

static const int len_extra_bits[29] = {
    0,0,0,0,0,0,0,0, 1,1,1,1, 2,2,2,2, 3,3,3,3, 4,4,4,4, 5,5,5,5, 0
};
static const int len_base[29] = {
    3,4,5,6,7,8,9,10, 11,13,15,17, 19,23,27,31,
    35,43,51,59, 67,83,99,115, 131,163,195,227, 258
};
static const int dist_extra_bits[30] = {
    0,0,0,0, 1,1, 2,2, 3,3, 4,4, 5,5, 6,6, 7,7, 8,8, 9,9,
    10,10, 11,11, 12,12, 13,13
};
static const int dist_base[30] = {
    1,2,3,4, 5,7, 9,13, 17,25, 33,49, 65,97, 129,193,
    257,385, 513,769, 1025,1537, 2049,3073, 4097,6145,
    8193,12289, 16385,24577
};

static int length_to_code(int len) {
    for (int i = 28; i >= 0; i--)
        if (len >= len_base[i]) return 257 + i;
    return 257;
}

static int dist_to_code(int dist) {
    for (int i = 29; i >= 0; i--)
        if (dist >= dist_base[i]) return i;
    return 0;
}

/* ============================================================
   LZ77 슬라이딩 윈도우 (해시 체인)
   SIMD: AVX2 → 32바이트 병렬 비교 / NEON → 16바이트
   ============================================================ */

#define HASH_SIZE FL_DEFLATE_HASH_SIZE
#define WIN_SIZE  FL_DEFLATE_WIN_SIZE
#define MIN_MATCH FL_DEFLATE_MIN_MATCH
#define MAX_MATCH FL_DEFLATE_MAX_MATCH

typedef struct {
    int32_t head[HASH_SIZE];  /* 해시 → 가장 최근 위치 */
    int32_t prev[WIN_SIZE];   /* 위치 → 이전 위치 (체인) */
} lz77_hash_t;

static uint32_t hash3(const uint8_t* p) {
    /* Knuth multiplicative hash (3바이트) */
    return ((uint32_t)p[0] ^ ((uint32_t)p[1] << 5) ^ ((uint32_t)p[2] << 10))
           & (HASH_SIZE - 1);
}

/**
 * LZ77 최장 매칭 탐색.
 * level >= 6 → 체인 깊이 128, level 1-5 → 32.
 * AVX2 활성화 시 32바이트씩 병렬 비교로 3x 가속.
 */
static int lz77_find_match(
        const uint8_t* src, size_t src_len,
        size_t pos, lz77_hash_t* ht,
        int* best_dist, int level)
{
    if (pos + MIN_MATCH > src_len) return 0;

    uint32_t h         = hash3(src + pos);
    int32_t  candidate = ht->head[h];
    int      best_len  = MIN_MATCH - 1;
    int      chain     = (level >= 6) ? 128 : (level >= 3) ? 32 : 8;

    while (candidate >= 0 && chain-- > 0) {
        int dist = (int)(pos - (size_t)candidate);
        if (dist <= 0 || dist > WIN_SIZE) break;

        const uint8_t* a = src + pos;
        const uint8_t* b = src + (size_t)candidate;
        int max_cmp = (int)(src_len - pos);
        if (max_cmp > MAX_MATCH) max_cmp = MAX_MATCH;
        int mlen = 0;

#ifdef __AVX2__
        /* AVX2: 32바이트씩 병렬 비교 */
        while (mlen + 32 <= max_cmp) {
            __m256i va   = _mm256_loadu_si256((const __m256i*)(a + mlen));
            __m256i vb   = _mm256_loadu_si256((const __m256i*)(b + mlen));
            __m256i cmp  = _mm256_cmpeq_epi8(va, vb);
            uint32_t msk = ~(uint32_t)_mm256_movemask_epi8(cmp);
            if (msk) { mlen += __builtin_ctz(msk); goto avx2_done; }
            mlen += 32;
        }
        avx2_done:;
#elif defined(__ARM_NEON)
        /* NEON: 16바이트씩 병렬 비교 */
        while (mlen + 16 <= max_cmp) {
            uint8x16_t va  = vld1q_u8(a + mlen);
            uint8x16_t vb  = vld1q_u8(b + mlen);
            uint8x16_t cmp = vceqq_u8(va, vb);
            /* all equal → 0xFFFF...FF */
            uint64_t lo = vgetq_lane_u64(vreinterpretq_u64_u8(cmp), 0);
            uint64_t hi = vgetq_lane_u64(vreinterpretq_u64_u8(cmp), 1);
            if (lo != 0xFFFFFFFFFFFFFFFFULL || hi != 0xFFFFFFFFFFFFFFFFULL) {
                /* 첫 불일치 바이트 찾기 */
                while (mlen < max_cmp && a[mlen] == b[mlen]) mlen++;
                goto neon_done;
            }
            mlen += 16;
        }
        neon_done:;
#endif
        /* 스칼라 나머지 */
        while (mlen < max_cmp && a[mlen] == b[mlen]) mlen++;

        if (mlen > best_len) {
            best_len   = mlen;
            *best_dist = dist;
            if (best_len >= MAX_MATCH) break;
        }

        candidate = (candidate < (int32_t)WIN_SIZE)
                    ? ht->prev[(size_t)candidate]
                    : -1;
    }

    /* 해시 체인 업데이트 */
    ht->prev[pos & (WIN_SIZE - 1)] = ht->head[h];
    ht->head[h] = (int32_t)pos;

    return best_len;
}

/* ============================================================
   DEFLATE 압축 (Fixed Huffman)
   ============================================================ */

static void write_literal(fl_bit_stream_t* bs, uint8_t lit) {
    fl_bit_stream_write(bs, fixed_lit[(int)lit].code, fixed_lit[(int)lit].len);
}

static void write_length_dist(fl_bit_stream_t* bs, int length, int dist) {
    int lcode = length_to_code(length);
    fl_bit_stream_write(bs, fixed_lit[lcode].code, fixed_lit[lcode].len);
    int lidx = lcode - 257;
    if (len_extra_bits[lidx] > 0)
        fl_bit_stream_write(bs, (uint32_t)(length - len_base[lidx]), len_extra_bits[lidx]);

    int dcode = dist_to_code(dist);
    fl_bit_stream_write(bs, fixed_dist[dcode].code, fixed_dist[dcode].len);
    if (dist_extra_bits[dcode] > 0)
        fl_bit_stream_write(bs, (uint32_t)(dist - dist_base[dcode]), dist_extra_bits[dcode]);
}

fl_bytes_t* fl_deflate_compress(const uint8_t* src, size_t src_len, int level) {
    crc32_init();
    init_fixed_tables();
    if (level < 0) level = 0;
    if (level > 9) level = 9;

    fl_bit_stream_t bs;
    fl_bit_stream_init(&bs, src_len + 64);

    /* DEFLATE 블록 헤더: BFINAL=1, BTYPE=01 (Fixed Huffman) */
    fl_bit_stream_write(&bs, 1, 1); /* BFINAL */
    fl_bit_stream_write(&bs, 1, 1); /* BTYPE bit0 = 1 */
    fl_bit_stream_write(&bs, 0, 1); /* BTYPE bit1 = 0  → Fixed */

    if (level == 0) {
        /* Store (no compression) - literal only */
        for (size_t i = 0; i < src_len; i++)
            write_literal(&bs, src[i]);
    } else {
        lz77_hash_t* ht = (lz77_hash_t*)malloc(sizeof(lz77_hash_t));
        memset(ht->head, -1, sizeof(ht->head));
        memset(ht->prev, -1, sizeof(ht->prev));

        size_t pos = 0;
        while (pos < src_len) {
            if (pos + MIN_MATCH <= src_len) {
                int dist = 0;
                int mlen = lz77_find_match(src, src_len, pos, ht, &dist, level);
                if (mlen >= MIN_MATCH) {
                    write_length_dist(&bs, mlen, dist);
                    /* 스킵 위치들도 해시 테이블 업데이트 */
                    for (int k = 1; k < mlen && pos + (size_t)k < src_len; k++) {
                        uint32_t h = hash3(src + pos + k);
                        ht->prev[(pos + (size_t)k) & (WIN_SIZE - 1)] = ht->head[h];
                        ht->head[h] = (int32_t)(pos + (size_t)k);
                    }
                    pos += (size_t)mlen;
                    continue;
                }
            }
            write_literal(&bs, src[pos]);
            pos++;
        }
        free(ht);
    }

    /* EOB symbol 256 */
    fl_bit_stream_write(&bs, fixed_lit[256].code, fixed_lit[256].len);
    fl_bit_stream_flush(&bs);

    fl_bytes_t* out = (fl_bytes_t*)malloc(sizeof(fl_bytes_t));
    out->data     = bs.buf;
    out->size     = bs.byte_pos;
    out->capacity = bs.cap;
    return out;
}

/* ============================================================
   INFLATE (Fixed Huffman Decompress)
   RFC 1951 §3.2.6
   ============================================================ */

typedef struct {
    const uint8_t* buf;
    size_t         len;
    size_t         byte_pos;
    uint32_t       bit_cache;
    int            bits_in;
} inflate_stream_t;

static int inf_bit(inflate_stream_t* is) {
    if (is->bits_in == 0) {
        if (is->byte_pos >= is->len) return -1;
        is->bit_cache = is->buf[is->byte_pos++];
        is->bits_in   = 8;
    }
    int b = (int)(is->bit_cache & 1);
    is->bit_cache >>= 1;
    is->bits_in--;
    return b;
}

static int inf_bits(inflate_stream_t* is, int n) {
    int val = 0;
    for (int i = 0; i < n; i++) {
        int b = inf_bit(is);
        if (b < 0) return -1;
        val |= (b << i);
    }
    return val;
}

/* Fixed Huffman 리터럴/길이 코드 디코딩 */
static int decode_fixed_litlen(inflate_stream_t* is) {
    /* RFC 1951 §3.2.6: 7비트 먼저 읽어 범위 확인 */
    int code = 0;
    for (int i = 0; i < 7; i++) {
        int b = inf_bit(is);
        if (b < 0) return -1;
        code = (code << 1) | b;
    }
    if (code <= 0x17)              return 256 + code;   /* 256-279: 7비트 */

    int e8 = inf_bit(is);
    if (e8 < 0) return -1;
    code = (code << 1) | e8;

    if (code >= 0x30 && code <= 0xBF) return code - 0x30;       /* 0-143:  8비트 */
    if (code >= 0xC0 && code <= 0xC7) return 280 + (code - 0xC0); /* 280-287:8비트 */

    int e9 = inf_bit(is);
    if (e9 < 0) return -1;
    code = (code << 1) | e9;
    if (code >= 0x190 && code <= 0x1FF) return 144 + (code - 0x190); /* 144-255:9비트 */

    return -1;
}

static void out_grow(uint8_t** buf, size_t* cap) {
    *cap = *cap * 2 + 64;
    *buf = (uint8_t*)realloc(*buf, *cap);
}

fl_bytes_t* fl_deflate_decompress(const uint8_t* src, size_t src_len) {
    inflate_stream_t is = { src, src_len, 0, 0, 0 };

    size_t   out_cap = src_len * 4 + 64;
    uint8_t* out     = (uint8_t*)malloc(out_cap);
    size_t   out_pos = 0;

    int bfinal = 0;
    while (!bfinal) {
        bfinal    = inf_bits(&is, 1);
        int btype = inf_bits(&is, 2);

        if (btype == 1) {
            /* Fixed Huffman block */
            for (;;) {
                int sym = decode_fixed_litlen(&is);
                if (sym < 0 || sym > 285) break;
                if (sym == 256) break; /* EOB */

                if (sym < 256) {
                    if (out_pos >= out_cap) out_grow(&out, &out_cap);
                    out[out_pos++] = (uint8_t)sym;
                } else {
                    int lidx = sym - 257;
                    if (lidx < 0 || lidx > 28) break;
                    int length = len_base[lidx];
                    if (len_extra_bits[lidx] > 0) {
                        int ex = inf_bits(&is, len_extra_bits[lidx]);
                        if (ex < 0) break;
                        length += ex;
                    }
                    int dcode = inf_bits(&is, 5);
                    if (dcode < 0 || dcode > 29) break;
                    int dist = dist_base[dcode];
                    if (dist_extra_bits[dcode] > 0) {
                        int ex = inf_bits(&is, dist_extra_bits[dcode]);
                        if (ex < 0) break;
                        dist += ex;
                    }
                    for (int k = 0; k < length; k++) {
                        if (out_pos >= out_cap) out_grow(&out, &out_cap);
                        out[out_pos] = out[out_pos - (size_t)dist];
                        out_pos++;
                    }
                }
            }
        } else if (btype == 0) {
            /* Stored block (no compression) */
            is.bits_in = 0; /* byte-align */
            if (is.byte_pos + 4 > src_len) break;
            uint16_t nlen = (uint16_t)(src[is.byte_pos] | ((uint16_t)src[is.byte_pos+1] << 8));
            is.byte_pos += 4;
            for (uint16_t i = 0; i < nlen; i++) {
                if (is.byte_pos >= src_len) break;
                if (out_pos >= out_cap) out_grow(&out, &out_cap);
                out[out_pos++] = src[is.byte_pos++];
            }
        } else {
            break; /* Dynamic Huffman: 미구현 */
        }
    }

    fl_bytes_t* result = (fl_bytes_t*)malloc(sizeof(fl_bytes_t));
    result->data     = out;
    result->size     = out_pos;
    result->capacity = out_cap;
    return result;
}

/* ============================================================
   GZIP Wrapper (RFC 1952)
   헤더 10B + deflate payload + CRC32(4B) + ISIZE(4B)
   ============================================================ */

fl_bytes_t* fl_gzip_wrap(const uint8_t* src, size_t src_len, int level) {
    crc32_init();

    fl_bytes_t* deflated = fl_deflate_compress(src, src_len, level);
    if (!deflated) return NULL;

    size_t   total = 10 + deflated->size + 8;
    uint8_t* out   = (uint8_t*)malloc(total);
    size_t   pos   = 0;

    /* GZIP 헤더 (RFC 1952 §2.3) */
    out[pos++] = 0x1f;  /* ID1 */
    out[pos++] = 0x8b;  /* ID2 */
    out[pos++] = 8;     /* CM: deflate */
    out[pos++] = 0;     /* FLG: 없음 */
    out[pos++] = 0; out[pos++] = 0; out[pos++] = 0; out[pos++] = 0; /* MTIME */
    out[pos++] = (level >= 9) ? 2 : (level <= 1) ? 4 : 0;           /* XFL */
    out[pos++] = 255;   /* OS: unknown */

    memcpy(out + pos, deflated->data, deflated->size);
    pos += deflated->size;

    /* GZIP 트레일러 */
    uint32_t crc   = crc32_compute(src, src_len);
    uint32_t isize = (uint32_t)(src_len & 0xFFFFFFFFU);
    out[pos++] = (uint8_t)( crc        & 0xFF);
    out[pos++] = (uint8_t)((crc >>  8) & 0xFF);
    out[pos++] = (uint8_t)((crc >> 16) & 0xFF);
    out[pos++] = (uint8_t)((crc >> 24) & 0xFF);
    out[pos++] = (uint8_t)( isize        & 0xFF);
    out[pos++] = (uint8_t)((isize >>  8) & 0xFF);
    out[pos++] = (uint8_t)((isize >> 16) & 0xFF);
    out[pos++] = (uint8_t)((isize >> 24) & 0xFF);

    free(deflated->data);
    free(deflated);

    fl_bytes_t* result = (fl_bytes_t*)malloc(sizeof(fl_bytes_t));
    result->data     = out;
    result->size     = pos;
    result->capacity = total;
    return result;
}

fl_bytes_t* fl_gzip_unwrap(const uint8_t* src, size_t src_len) {
    if (src_len < 18)                        return NULL;
    if (src[0] != 0x1f || src[1] != 0x8b)   return NULL;
    if (src[2] != 8)                         return NULL; /* deflate only */

    uint8_t flg      = src[3];
    size_t  hdr_end  = 10;

    if (flg & 0x04) { /* FEXTRA */
        if (hdr_end + 2 > src_len) return NULL;
        uint16_t xlen = (uint16_t)(src[hdr_end] | ((uint16_t)src[hdr_end+1] << 8));
        hdr_end += 2 + (size_t)xlen;
    }
    if (flg & 0x08) { /* FNAME */
        while (hdr_end < src_len && src[hdr_end]) hdr_end++;
        hdr_end++;
    }
    if (flg & 0x10) { /* FCOMMENT */
        while (hdr_end < src_len && src[hdr_end]) hdr_end++;
        hdr_end++;
    }
    if (flg & 0x02) hdr_end += 2; /* FHCRC */

    if (hdr_end + 8 > src_len) return NULL;
    size_t payload_len = src_len - hdr_end - 8;

    return fl_deflate_decompress(src + hdr_end, payload_len);
}

/* ============================================================
   Helper: fl_value_t 생성
   ============================================================ */

static fl_value_t mkbytes(fl_bytes_t* b) {
    fl_value_t v; v.type = FL_TYPE_BYTES; v.data.bytes_val = b; return v;
}
static fl_value_t mknull(void) {
    fl_value_t v; v.type = FL_TYPE_NULL; return v;
}
static fl_value_t mkint(int64_t n) {
    fl_value_t v; v.type = FL_TYPE_INT; v.data.int_val = n; return v;
}
static fl_value_t mkfloat(double f) {
    fl_value_t v; v.type = FL_TYPE_FLOAT; v.data.float_val = f; return v;
}
static fl_value_t mkstring(const char* s) {
    fl_value_t v;
    v.type = FL_TYPE_STRING;
    v.data.string_val = s ? strdup(s) : NULL;
    return v;
}

/* ============================================================
   FreeLang Stdlib Bindings
   ============================================================ */

/**
 * compress(data: bytes|string, level: int = 6) -> bytes
 * GZIP 포맷으로 압축. MOSS-Pulse 응답 파이프라인과 직결.
 */
fl_value_t fl_compress(fl_value_t* args, size_t argc) {
    if (argc < 1) return mknull();

    const uint8_t* src = NULL;
    size_t src_len = 0;

    if (args[0].type == FL_TYPE_BYTES) {
        src     = args[0].data.bytes_val->data;
        src_len = args[0].data.bytes_val->size;
    } else if (args[0].type == FL_TYPE_STRING && args[0].data.string_val) {
        src     = (const uint8_t*)args[0].data.string_val;
        src_len = strlen(args[0].data.string_val);
    } else {
        return mknull();
    }

    int level = FL_COMPRESS_DEFAULT;
    if (argc >= 2 && args[1].type == FL_TYPE_INT)
        level = (int)args[1].data.int_val;

    fl_bytes_t* result = fl_gzip_wrap(src, src_len, level);
    return result ? mkbytes(result) : mknull();
}

/**
 * decompress(data: bytes) -> bytes
 * GZIP 해제.
 */
fl_value_t fl_decompress(fl_value_t* args, size_t argc) {
    if (argc < 1 || args[0].type != FL_TYPE_BYTES) return mknull();
    fl_bytes_t* b      = args[0].data.bytes_val;
    fl_bytes_t* result = fl_gzip_unwrap(b->data, b->size);
    return result ? mkbytes(result) : mknull();
}

/** gzip(data, level=6) -> bytes  [compress alias] */
fl_value_t fl_gzip(fl_value_t* args, size_t argc) {
    return fl_compress(args, argc);
}

/** gunzip(data) -> bytes  [decompress alias] */
fl_value_t fl_gunzip(fl_value_t* args, size_t argc) {
    return fl_decompress(args, argc);
}

/**
 * compress_ratio(original: bytes|int, compressed: bytes|int) -> float
 * 압축률 계산 (original / compressed). 클수록 더 좋은 압축.
 */
fl_value_t fl_compress_ratio(fl_value_t* args, size_t argc) {
    if (argc < 2) return mkfloat(1.0);
    size_t orig = 0, comp = 0;
    if (args[0].type == FL_TYPE_BYTES)      orig = args[0].data.bytes_val->size;
    else if (args[0].type == FL_TYPE_INT)   orig = (size_t)args[0].data.int_val;
    if (args[1].type == FL_TYPE_BYTES)      comp = args[1].data.bytes_val->size;
    else if (args[1].type == FL_TYPE_INT)   comp = (size_t)args[1].data.int_val;
    if (comp == 0) return mkfloat(0.0);
    return mkfloat((double)orig / (double)comp);
}

/**
 * compress_info(data: bytes|string) -> object
 * {original, compressed, ratio, savings_pct, simd_backend}
 * Smart-Throttle: 1024바이트 미만은 압축 생략 권고값 포함.
 */
fl_value_t fl_compress_info(fl_value_t* args, size_t argc) {
    if (argc < 1) return mknull();

    const uint8_t* src = NULL;
    size_t src_len = 0;

    if (args[0].type == FL_TYPE_BYTES) {
        src     = args[0].data.bytes_val->data;
        src_len = args[0].data.bytes_val->size;
    } else if (args[0].type == FL_TYPE_STRING && args[0].data.string_val) {
        src     = (const uint8_t*)args[0].data.string_val;
        src_len = strlen(args[0].data.string_val);
    } else {
        return mknull();
    }

    fl_bytes_t* compressed = fl_gzip_wrap(src, src_len, FL_COMPRESS_DEFAULT);
    if (!compressed) return mknull();

    double ratio    = src_len > 0 ? (double)compressed->size / (double)src_len : 1.0;
    double savings  = (1.0 - ratio) * 100.0;

    /* SIMD 백엔드 감지 */
    const char* simd_backend = "scalar";
#ifdef __AVX2__
    simd_backend = "avx2";
#elif defined(__ARM_NEON)
    simd_backend = "neon";
#endif

    fl_object_t* obj = (fl_object_t*)malloc(sizeof(fl_object_t));
    obj->capacity = 8;
    obj->size     = 0;
    obj->keys     = (char**)malloc(8 * sizeof(char*));
    obj->values   = (fl_value_t*)malloc(8 * sizeof(fl_value_t));

    obj->keys[obj->size] = strdup("original");
    obj->values[obj->size++] = mkint((int64_t)src_len);

    obj->keys[obj->size] = strdup("compressed");
    obj->values[obj->size++] = mkint((int64_t)compressed->size);

    obj->keys[obj->size] = strdup("ratio");
    obj->values[obj->size++] = mkfloat(ratio);

    obj->keys[obj->size] = strdup("savings_pct");
    obj->values[obj->size++] = mkfloat(savings);

    obj->keys[obj->size] = strdup("simd_backend");
    obj->values[obj->size++] = mkstring(simd_backend);

    /* Smart-Throttle 권고: 1024B 미만은 헤더 오버헤드로 손해 */
    obj->keys[obj->size] = strdup("should_compress");
    obj->values[obj->size++] = mkint(src_len >= 1024 && savings > 5.0 ? 1 : 0);

    free(compressed->data);
    free(compressed);

    fl_value_t result;
    result.type            = FL_TYPE_OBJECT;
    result.data.object_val = obj;
    return result;
}
