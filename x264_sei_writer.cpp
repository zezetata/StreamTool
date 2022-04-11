//
// Created by Leo_Liu on 2022/4/11.
//

#include "x264_sei_write.h"

#include <stdio.h>
#include <iostream>

#define MAY_ALIAS __attribute__((may_alias))

typedef union {
    uint32_t i;
    uint16_t w[2];
    uint8_t b[4];
} MAY_ALIAS x264_union32_t;

#define WORD_SIZE sizeof(void*)

#define M32(src) (((x264_union32_t*)(src))->i)


typedef struct bs_s {
    uint8_t *p_start;
    uint8_t *p;
    uint8_t *p_end;

    uintptr_t cur_bits;
    int i_left;    /* i_count number of available bits */
    int i_bits_encoded; /* RD only */
} bs_t;


static inline uint32_t endian_fix32(uint32_t x) {
    return (x << 24) + ((x << 8) & 0xff0000) + ((x >> 8) & 0xff00) + (x >> 24);
}


static inline uint64_t endian_fix64(uint64_t x) {
    return endian_fix32(x >> 32) + ((uint64_t) endian_fix32(x) << 32);
}


static inline uintptr_t endian_fix(uintptr_t x) {
    return WORD_SIZE == 8 ? endian_fix64(x) : endian_fix32(x);
}


static inline void bs_realign(bs_t *s) {
    int offset = ((intptr_t) s->p & 3);
    if (offset) {
        s->p = (uint8_t *) s->p - offset;
        s->i_left = (WORD_SIZE - offset) * 8;
        s->cur_bits = endian_fix32(M32(s->p));
        s->cur_bits >>= (4 - offset) * 8;
    }
}


static inline void bs_write(bs_t *s, int i_count, uint32_t i_bits) {
    if (WORD_SIZE == 8) {
        s->cur_bits = (s->cur_bits << i_count) | i_bits;
        s->i_left -= i_count;
        if (s->i_left <= 32) {
            M32(s->p) = s->cur_bits >> (32 - s->i_left);
            s->i_left += 32;
            s->p += 4;
        }
    } else {
        if (i_count < s->i_left) {
            s->cur_bits = (s->cur_bits << i_count) | i_bits;
            s->i_left -= i_count;
        } else {
            i_count -= s->i_left;
            s->cur_bits = (s->cur_bits << s->i_left) | (i_bits >> i_count);
            M32(s->p) = endian_fix(s->cur_bits);
            s->p += 4;
            s->cur_bits = i_bits;
            s->i_left = 32 - i_count;
        }
    }
}


static inline void bs_write1(bs_t *s, uint32_t i_bit) {
    s->cur_bits <<= 1;
    s->cur_bits |= i_bit;
    s->i_left--;
    if (s->i_left == WORD_SIZE * 8 - 32) {
        M32(s->p) = endian_fix32(s->cur_bits);
        s->p += 4;
        s->i_left = WORD_SIZE * 8;
    }
}


static inline void bs_flush(bs_t *s) {
    M32(s->p) = endian_fix32(s->cur_bits << (s->i_left & 31));
    s->p += WORD_SIZE - (s->i_left >> 3);
    s->i_left = WORD_SIZE * 8;
}


static inline void bs_rbsp_trailing(bs_t *s) {
    bs_write1(s, 1);
    bs_write(s, s->i_left & 7, 0);
}


void x264_sei_write(bs_t *s, uint8_t *payload, int payload_size, int payload_type) {
    int i;

    bs_realign(s);

    for (i = 0; i <= payload_type - 255; i += 255)
        bs_write(s, 8, 255);
    bs_write(s, 8, payload_type - i);

    for (i = 0; i <= payload_size - 255; i += 255)
        bs_write(s, 8, 255);
    bs_write(s, 8, payload_size - i);

    for (i = 0; i < payload_size; i++)
        bs_write(s, 8, payload[i]);

    bs_rbsp_trailing(s);
    bs_flush(s);
}

