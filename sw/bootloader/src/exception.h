#ifndef EXCEPTION_H__
#define EXCEPTION_H__


#include <stdint.h>


typedef union {
    uint64_t u64;
    struct {
        uint32_t u32_h;
        uint32_t u32;
    };
} uint64_32_t;

typedef struct {
    uint64_32_t zr;
    uint64_32_t at;
    uint64_32_t v0;
    uint64_32_t v1;
    uint64_32_t a0;
    uint64_32_t a1;
    uint64_32_t a2;
    uint64_32_t a3;
    uint64_32_t t0;
    uint64_32_t t1;
    uint64_32_t t2;
    uint64_32_t t3;
    uint64_32_t t4;
    uint64_32_t t5;
    uint64_32_t t6;
    uint64_32_t t7;
    uint64_32_t s0;
    uint64_32_t s1;
    uint64_32_t s2;
    uint64_32_t s3;
    uint64_32_t s4;
    uint64_32_t s5;
    uint64_32_t s6;
    uint64_32_t s7;
    uint64_32_t t8;
    uint64_32_t t9;
    uint64_32_t k0;
    uint64_32_t k1;
    uint64_32_t gp;
    uint64_32_t sp;
    uint64_32_t s8;
    uint64_32_t ra;
    uint32_t sr;
    uint32_t cr;
    uint64_32_t epc;
    uint64_32_t badvaddr;
} exception_t;


#endif
