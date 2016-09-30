/* -*- C -*-
 *
 * Copyright 2016 Sandia Corporation. Under the terms of Contract
 * DE-AC04-94AL85000 with Sandia Corporation, the U.S.  Government
 * retains certain rights in this software.
 *
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * This software is available to you under the BSD license.
 *
 * This file is part of the Sandia OpenSHMEM software package. For license
 * information, see the LICENSE file in the top level directory of the
 * distribution.
 *
 */

#ifndef SHMEM_THREAD_H
#define SHMEM_THREAD_H

#include <stddef.h>
#include <stdio.h>
#include <shmem.h>

extern _Thread_local int             thread_is_registered;
extern _Thread_local shmemx_domain_t thread_domain;
extern _Thread_local shmemx_ctx_t    thread_ctx;

extern _Atomic int num_threads;
extern _Atomic int barrier_cnt;
extern _Atomic int barrier_complete;

static inline void shmem_thread_register(void) {
    int err;

    shmemx_domain_create(SHMEMX_THREAD_MULTIPLE, 1, &thread_domain);
    err = shmemx_ctx_create(thread_domain, &thread_ctx);

    if (err) {
        fprintf(stderr, "Error: shmem_thread_register ctx creation failed (err %d, pe %d)\n",
                err, shmem_my_pe());
        shmem_global_exit(err);
    }

    thread_is_registered = 1;
    ++num_threads;
}

static inline void shmem_thread_unregister(void) {
    shmemx_ctx_destroy(thread_ctx);
    shmemx_domain_destroy(1, &thread_domain);

    thread_is_registered = 0;
    --num_threads;
}

static inline int shmem_thread_is_registered() {
    return (thread_is_registered != 0);
}

static inline void shmem_thread_barrier(int expected) {
    int cnt;

    while (barrier_complete != 0) ;

    cnt = ++barrier_cnt;

    if (cnt == expected) barrier_complete = 1;
    else while (!barrier_complete) ;

    cnt = --barrier_cnt;
    if (cnt == 0) barrier_complete = 0;
}

static inline void shmem_thread_fence(void) {
    shmemx_ctx_fence(thread_ctx);
}

static inline void shmem_thread_quiet(void) {
    shmemx_ctx_quiet(thread_ctx);
}

#define SHMEM_EVAL_MACRO_FOR_RMA(decl,END) \
  decl(float,      float) END           \
  decl(double,     double) END          \
  decl(longdouble, long double) END     \
  decl(char,       char) END            \
  decl(short,      short) END           \
  decl(int,        int) END             \
  decl(long,       long) END            \
  decl(longlong,   long long)

#define SHMEM_EVAL_MACRO_FOR_AMO(decl,END) \
  decl(int,        int) END             \
  decl(long,       long) END            \
  decl(longlong,   long long)

#define SHMEM_EVAL_MACRO_FOR_EXTENDED_AMO(decl,END) \
  SHMEM_EVAL_MACRO_FOR_AMO(decl,END) END            \
  decl(float,  float) END                        \
  decl(double, double)

#define SHMEM_EVAL_MACRO_FOR_INTS(decl,END) \
  decl(short,    short) END              \
  decl(int,      int) END                \
  decl(long,     long) END               \
  decl(longlong, long long)

#define SHMEM_EVAL_MACRO_FOR_FLOATS(decl,END) \
  decl(float,     float) END               \
  decl(double,     double) END             \
  decl(longdouble, long double)

#define SHMEM_EVAL_MACRO_FOR_CMPLX(decl,END) \
  decl(complexf, float complex) END       \
  decl(complexd, double complex)

#define SHMEM_EVAL_MACRO_FOR_SIZES(decl,END) \
  decl(8,    1*sizeof(uint8_t)) END       \
  decl(16,   2*sizeof(uint8_t)) END       \
  decl(32,   4*sizeof(uint8_t)) END       \
  decl(64,   8*sizeof(uint8_t)) END       \
  decl(128, 16*sizeof(uint8_t))

#define SHMEM_DEFINE_FOR_RMA(decl) SHMEM_EVAL_MACRO_FOR_RMA(decl,)
#define SHMEM_DEFINE_FOR_AMO(decl) SHMEM_EVAL_MACRO_FOR_AMO(decl,)
#define SHMEM_DEFINE_FOR_EXTENDED_AMO(decl) SHMEM_EVAL_MACRO_FOR_EXTENDED_AMO(decl,)
#define SHMEM_DEFINE_FOR_INTS(decl) SHMEM_EVAL_MACRO_FOR_INTS(decl,)
#define SHMEM_DEFINE_FOR_FLOATS(decl) SHMEM_EVAL_MACRO_FOR_FLOATS(decl,)
#define SHMEM_DEFINE_FOR_CMPLX(decl) SHMEM_EVAL_MACRO_FOR_CMPLX(decl,)
#define SHMEM_DEFINE_FOR_SIZES(decl) SHMEM_EVAL_MACRO_FOR_SIZES(decl,)

/* 8.3: Elemental Data Put Routines */
#define SHMEM_C_P(TYPENAME,TYPE) \
    static inline void shmem_thread_##TYPENAME##_p(TYPE *addr, TYPE value, int pe) {  \
        shmemx_ctx_##TYPENAME##_p(addr, value, pe, thread_ctx);          \
    }
SHMEM_DEFINE_FOR_RMA(SHMEM_C_P)

/* 8.3: Block Data Put Routines */
#define SHMEM_C_PUT(TYPENAME,TYPE) \
    static inline void shmem_thread_##TYPENAME##_put(TYPE *target, const TYPE *source, \
                                size_t nelems, int pe) {                 \
        shmemx_ctx_##TYPENAME##_put(target, source, nelems, pe, thread_ctx); \
    }
SHMEM_DEFINE_FOR_RMA(SHMEM_C_PUT)

#define SHMEM_C_PUT_N(SIZE,NBYTES) \
    static inline void shmem_thread_put##SIZE(void* target, const void *source,       \
                         size_t nelems, int pe) {                       \
        shmemx_ctx_put##SIZE(target, source, nelems, pe, thread_ctx);    \
    }
SHMEM_DEFINE_FOR_SIZES(SHMEM_C_PUT_N)
SHMEM_C_PUT_N(mem,1)

/* 8.3: Strided Put Routines */
#define SHMEM_C_IPUT(TYPENAME,TYPE) \
    static inline void shmem_thread_##TYPENAME##_iput(TYPE *target, const TYPE *source, \
                               ptrdiff_t tst, ptrdiff_t sst,            \
                               size_t len, int pe) {                    \
        shmemx_ctx_##TYPENAME##_iput(target, source, tst, sst, len, pe, \
                                     thread_ctx);               \
    }
SHMEM_DEFINE_FOR_RMA(SHMEM_C_IPUT)

#define SHMEM_C_IPUT_N(SIZE,NBYTES) \
    static inline void shmem_thread_iput##SIZE(void *target, const void *source,        \
                        ptrdiff_t tst, ptrdiff_t sst, size_t len,       \
                        int pe) {                                       \
        shmemx_ctx_iput##SIZE(target, source, tst, sst, len, pe,        \
                             thread_ctx);                               \
    }
SHMEM_DEFINE_FOR_SIZES(SHMEM_C_IPUT_N)

/* 8.3: Elemental Data Get Routines */
#define SHMEM_C_G(TYPENAME,TYPE) \
    static inline TYPE shmem_thread_##TYPENAME##_g(const TYPE *addr, int pe) {        \
        return shmemx_ctx_##TYPENAME##_g(addr, pe, thread_ctx);          \
    }
SHMEM_DEFINE_FOR_RMA(SHMEM_C_G)

/* 8.3: Block Data Get Routines */
#define SHMEM_C_GET(TYPENAME,TYPE) \
    static inline void shmem_thread_##TYPENAME##_get(TYPE *target, const TYPE *source,        \
                                size_t nelems,int pe) {                         \
        shmemx_ctx_##TYPENAME##_get(target, source, nelems, pe, thread_ctx);     \
    }
SHMEM_DEFINE_FOR_RMA(SHMEM_C_GET)

#define SHMEM_C_GET_N(SIZE,NBYTES) \
    static inline void shmem_thread_get##SIZE(void* target, const void *source,       \
                         size_t nelems, int pe) {                       \
        shmemx_ctx_get##SIZE(target, source, nelems, pe, thread_ctx);    \
    }
SHMEM_DEFINE_FOR_SIZES(SHMEM_C_GET_N)
SHMEM_C_GET_N(mem,1)

/* 8.3: Strided Get Routines */
#define SHMEM_C_IGET(TYPENAME,TYPE) \
    static inline void shmem_thread_##TYPENAME##_iget(TYPE *target, const TYPE *source, \
                                 ptrdiff_t tst, ptrdiff_t sst,          \
                                 size_t nelems, int pe) {               \
        shmemx_ctx_##TYPENAME##_iget(target, source, tst, sst, nelems,   \
                                    pe, thread_ctx);                    \
    }
SHMEM_DEFINE_FOR_RMA(SHMEM_C_IGET)

#define SHMEM_C_IGET_N(SIZE,NBYTES) \
    static inline void shmem_thread_iget##SIZE(void* target, const void *source,      \
                          ptrdiff_t tst, ptrdiff_t sst,                 \
                          size_t nelems, int pe) {                      \
        shmemx_ctx_iget##SIZE(target, source, tst, sst, nelems, pe,      \
                             thread_ctx);                               \
    }
SHMEM_DEFINE_FOR_SIZES(SHMEM_C_IGET_N)

/* 8.4: Nonblocking remote memory access routines -- Put */
#define SHMEM_C_PUT_NBI(TYPENAME,TYPE) \
    static inline void shmem_thread_##TYPENAME##_put_nbi(TYPE *target, const TYPE *source,\
                                    size_t nelems, int pe) {            \
        shmemx_ctx_##TYPENAME##_put_nbi(target, source, nelems, pe,      \
                                       thread_ctx);                     \
    }
SHMEM_DEFINE_FOR_RMA(SHMEM_C_PUT_NBI)

#define SHMEM_C_PUT_N_NBI(SIZE,NBYTES) \
    static inline void shmem_thread_put##SIZE##_nbi(void* target, const void *source, \
                               size_t nelems, int pe) {                 \
        shmemx_ctx_put##SIZE##_nbi(target, source, nelems, pe,           \
                                       thread_ctx);                     \
    }
SHMEM_DEFINE_FOR_SIZES(SHMEM_C_PUT_N_NBI)
SHMEM_C_PUT_N_NBI(mem,1)

/* 8.4: Nonblocking remote memory access routines -- Get */
#define SHMEM_C_GET_NBI(TYPENAME,TYPE) \
    static inline void shmem_thread_##TYPENAME##_get_nbi(TYPE *target, const TYPE *source,\
                                    size_t nelems, int pe) {            \
        shmemx_ctx_##TYPENAME##_get_nbi(target, source, nelems, pe,      \
                                       thread_ctx);                     \
    }
SHMEM_DEFINE_FOR_RMA(SHMEM_C_GET_NBI)

#define SHMEM_C_GET_N_NBI(SIZE,NBYTES) \
    static inline void shmem_thread_get##SIZE##_nbi(void* target, const void *source, \
                               size_t nelems, int pe) {                 \
        shmemx_ctx_get##SIZE##_nbi(target, source, nelems, pe,           \
                                       thread_ctx);                     \
    }
SHMEM_DEFINE_FOR_SIZES(SHMEM_C_GET_N_NBI)
SHMEM_C_GET_N_NBI(mem,1)

/* 8.4: Atomic Memory fetch-and-operate Routines -- Swap */
#define SHMEM_C_SWAP(TYPENAME,TYPE) \
  static inline TYPE shmem_thread_##TYPENAME##_swap(TYPE *target, TYPE value, int pe){\
      return shmemx_ctx_##TYPENAME##_swap(target, value, pe, thread_ctx);\
  }
SHMEM_DEFINE_FOR_EXTENDED_AMO(SHMEM_C_SWAP)

/* 8.4: Atomic Memory fetch-and-operate Routines -- Cswap */
#define SHMEM_C_CSWAP(TYPENAME,TYPE) \
  static inline TYPE shmem_thread_##TYPENAME##_cswap(TYPE *target, TYPE cond,         \
                                       TYPE value, int pe) {            \
      return shmemx_ctx_##TYPENAME##_cswap(target, cond, value, pe,      \
                                          thread_ctx);                  \
  }
SHMEM_DEFINE_FOR_AMO(SHMEM_C_CSWAP)

/* 8.4: Atomic Memory fetch-and-operate Routines -- Fetch and Add */
#define SHMEM_C_FADD(TYPENAME,TYPE) \
  static inline TYPE shmem_thread_##TYPENAME##_fadd(TYPE *target, TYPE value, int pe){\
      return shmemx_ctx_##TYPENAME##_fadd(target, value, pe, thread_ctx);\
  }
SHMEM_DEFINE_FOR_AMO(SHMEM_C_FADD)

/* 8.4: Atomic Memory fetch-and-operate Routines -- Fetch and Increment */
#define SHMEM_C_FINC(TYPENAME,TYPE) \
  static inline TYPE shmem_thread_##TYPENAME##_finc(TYPE *target, int pe) {           \
      return shmemx_ctx_##TYPENAME##_finc(target, pe, thread_ctx);       \
  }
SHMEM_DEFINE_FOR_AMO(SHMEM_C_FINC)

/* 8.4: Atomic Memory Operation Routines -- Add */
#define SHMEM_C_ADD(TYPENAME,TYPE) \
  static inline void shmem_thread_##TYPENAME##_add(TYPE *target, TYPE value, int pe) {\
      shmemx_ctx_##TYPENAME##_add(target, value, pe, thread_ctx);        \
  }
SHMEM_DEFINE_FOR_AMO(SHMEM_C_ADD)

/* 8.4: Atomic Memory Operation Routines -- Increment */
#define SHMEM_C_INC(TYPENAME,TYPE) \
  static inline void shmem_thread_##TYPENAME##_inc(TYPE *target, int pe) {            \
      shmemx_ctx_##TYPENAME##_inc(target, pe, thread_ctx);               \
  }
SHMEM_DEFINE_FOR_AMO(SHMEM_C_INC)

/* 8.4: Atomic fetch */
#define SHMEM_C_FETCH(TYPENAME,TYPE) \
  static inline TYPE shmem_thread_##TYPENAME##_fetch(const TYPE *target, int pe) {    \
      return shmemx_ctx_##TYPENAME##_fetch(target, pe, thread_ctx);             \
  }
SHMEM_DEFINE_FOR_EXTENDED_AMO(SHMEM_C_FETCH)

/* 8.4: Atomic set */
#define SHMEM_C_SET(TYPENAME,TYPE) \
  static inline void shmem_thread_##TYPENAME##_set(TYPE *target, TYPE value, int pe) {\
      shmemx_ctx_##TYPENAME##_set(target, value, pe, thread_ctx);        \
  }
SHMEM_DEFINE_FOR_EXTENDED_AMO(SHMEM_C_SET)

#undef SHMEM_C_P
#undef SHMEM_C_PUT
#undef SHMEM_C_PUT_N
#undef SHMEM_C_IPUT
#undef SHMEM_C_IPUT_N
#undef SHMEM_C_G
#undef SHMEM_C_GET
#undef SHMEM_C_GET_N
#undef SHMEM_C_IGET
#undef SHMEM_C_IGET_N
#undef SHMEM_C_PUT_NBI
#undef SHMEM_C_PUT_N_NBI
#undef SHMEM_C_GET_NBI
#undef SHMEM_C_GET_N_NBI
#undef SHMEM_C_SWAP
#undef SHMEM_C_CSWAP
#undef SHMEM_C_FADD
#undef SHMEM_C_FINC
#undef SHMEM_C_ADD
#undef SHMEM_C_INC
#undef SHMEM_C_FETCH
#undef SHMEM_C_SET

/* C++ overloaded declarations */
#ifdef __cplusplus
} /* extern "C" */
#ifdef complex
#undef complex
#endif

/* Blocking block, scalar, and block-strided put */
#define SHMEM_CXX_PUT(TYPENAME,TYPE) \
  static inline void shmem_thread_put(TYPE* dest, const TYPE* source,   \
                               size_t nelems, int pe) {                 \
    shmemx_ctx_##TYPENAME##_put(dest, source, nelems, pe, thread_ctx);   \
  }
SHMEM_DEFINE_FOR_RMA(SHMEM_CXX_PUT)

#define SHMEM_CXX_P(TYPENAME,TYPE) \
  static inline void shmem_thread_p(TYPE* dest, TYPE value, int pe) {   \
    shmemx_ctx_##TYPENAME##_p(dest, value, pe, thread_ctx);              \
  }
SHMEM_DEFINE_FOR_RMA(SHMEM_CXX_P)

#define SHMEM_CXX_IPUT(TYPENAME,TYPE) \
  static inline void shmem_thread_iput(TYPE *target, const TYPE *source,\
                                ptrdiff_t tst, ptrdiff_t sst,           \
                                size_t len, int pe) {                   \
    shmemx_ctx_##TYPENAME##_iput(target, source, tst, sst, len, pe,      \
                                thread_ctx);                            \
  }
SHMEM_DEFINE_FOR_RMA(SHMEM_CXX_IPUT)

/* Blocking block, scalar, and block-strided get */
#define SHMEM_CXX_GET(TYPENAME,TYPE) \
  static inline void shmem_thread_get(TYPE* dest, const TYPE* source,   \
                               size_t nelems, int pe) {                 \
    shmemx_ctx_##TYPENAME##_get(dest, source, nelems, pe, thread_ctx);   \
  }
SHMEM_DEFINE_FOR_RMA(SHMEM_CXX_GET)

#define SHMEM_CXX_G(TYPENAME,TYPE) \
  static inline TYPE shmem_thread_g(const TYPE* src, int pe) { \
    return shmemx_ctx_##TYPENAME##_g(src, pe, thread_ctx);               \
  }
SHMEM_DEFINE_FOR_RMA(SHMEM_CXX_G)

#define SHMEM_CXX_IGET(TYPENAME,TYPE) \
  static inline void shmem_thread_iget(TYPE *target, const TYPE *source,\
                                ptrdiff_t tst, ptrdiff_t sst,           \
                                size_t len, int pe) {                   \
    shmemx_ctx_##TYPENAME##_iget(target, source, tst, sst, len, pe,      \
                                thread_ctx);                            \
  }
SHMEM_DEFINE_FOR_RMA(SHMEM_CXX_IGET)

/* Nonblocking block put/get */
#define SHMEM_CXX_PUT_NBI(TYPENAME,TYPE) \
  static inline void shmem_thread_put_nbi(TYPE* dest, const TYPE* source, \
                                   size_t nelems, int pe) {             \
    shmemx_ctx_##TYPENAME##_put_nbi(dest, source, nelems, pe,            \
                                   thread_ctx);                         \
  }
SHMEM_DEFINE_FOR_RMA(SHMEM_CXX_PUT_NBI)

#define SHMEM_CXX_GET_NBI(TYPENAME,TYPE) \
  static inline void shmem_thread_get_nbi(TYPE* dest, const TYPE* source, \
                                   size_t nelems, int pe) {             \
    shmemx_ctx_##TYPENAME##_get_nbi(dest, source, nelems, pe,            \
                                   thread_ctx);                         \
  }
SHMEM_DEFINE_FOR_RMA(SHMEM_CXX_GET_NBI)


/* Atomics with standard AMO types */
#define SHMEM_CXX_ADD(TYPENAME,TYPE) \
  static inline void shmem_thread_add(TYPE *target, TYPE value, int pe) { \
    shmemx_ctx_##TYPENAME##_add(target, value, pe, thread_ctx);          \
  }
SHMEM_DEFINE_FOR_AMO(SHMEM_CXX_ADD)

#define SHMEM_CXX_CSWAP(TYPENAME,TYPE) \
  static inline TYPE shmem_thread_cswap(TYPE *target, TYPE cond, TYPE value,\
                                 int pe) {                              \
    return shmemx_ctx_##TYPENAME##_cswap(target, cond, value, pe,        \
                                        thread_ctx);                    \
  }
SHMEM_DEFINE_FOR_AMO(SHMEM_CXX_CSWAP)

#define SHMEM_CXX_FINC(TYPENAME,TYPE) \
  static inline TYPE shmem_thread_finc(TYPE *target, int pe) {          \
    return shmemx_ctx_##TYPENAME##_finc(target, pe, thread_ctx);         \
  }
SHMEM_DEFINE_FOR_AMO(SHMEM_CXX_FINC)

#define SHMEM_CXX_INC(TYPENAME,TYPE) \
  static inline void shmem_thread_inc(TYPE *target, int pe) {           \
    shmemx_ctx_##TYPENAME##_inc(target, pe, thread_ctx);                 \
  }
SHMEM_DEFINE_FOR_AMO(SHMEM_CXX_INC)

#define SHMEM_CXX_FADD(TYPENAME,TYPE) \
  static inline TYPE shmem_thread_fadd(TYPE *target, TYPE value, int pe) { \
    return shmemx_ctx_##TYPENAME##_fadd(target, value, pe, thread_ctx);  \
  }
SHMEM_DEFINE_FOR_AMO(SHMEM_CXX_FADD)

/* Atomics with extended AMO types */
#define SHMEM_CXX_SWAP(TYPENAME,TYPE) \
  static inline TYPE shmem_thread_swap(TYPE *target, TYPE value, int pe) { \
    return shmemx_ctx_##TYPENAME##_swap(target, value, pe, thread_ctx);  \
  }
SHMEM_DEFINE_FOR_EXTENDED_AMO(SHMEM_CXX_SWAP)

#define SHMEM_CXX_FETCH(TYPENAME,TYPE) \
  static inline TYPE shmem_thread_fetch(const TYPE *target, int pe) {   \
    return shmemx_ctx_##TYPENAME##_fetch(target, pe, thread_ctx);        \
  }
SHMEM_DEFINE_FOR_EXTENDED_AMO(SHMEM_CXX_FETCH)

#define SHMEM_CXX_SET(TYPENAME,TYPE) \
  static inline void shmem_thread_set(TYPE *target, TYPE value, int pe) { \
    shmemx_ctx_##TYPENAME##_set(target, value, pe, thread_ctx);          \
  }
SHMEM_DEFINE_FOR_EXTENDED_AMO(SHMEM_CXX_SET)

#undef SHMEM_CXX_PUT
#undef SHMEM_CXX_P
#undef SHMEM_CXX_IPUT
#undef SHMEM_CXX_GET
#undef SHMEM_CXX_G
#undef SHMEM_CXX_IGET
#undef SHMEM_CXX_PUT_NBI
#undef SHMEM_CXX_GET_NBI
#undef SHMEM_CXX_ADD
#undef SHMEM_CXX_CSWAP
#undef SHMEM_CXX_FINC
#undef SHMEM_CXX_INC
#undef SHMEM_CXX_FADD
#undef SHMEM_CXX_SWAP
#undef SHMEM_CXX_FETCH
#undef SHMEM_CXX_SET

/* C11 Generic Macros */
#elif (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(SHMEM_INTERNAL_INCLUDE))
#include <stdint.h>

/* Blocking block, scalar, and block-strided put */
#define shmem_thread_put(dest, source, nelems, pe)                     \
    _Generic(&*(dest),                                          \
             float*:            shmem_thread_float_put,                \
             double*:           shmem_thread_double_put,               \
             long double*:      shmem_thread_longdouble_put,           \
             char*:             shmem_thread_char_put,                 \
             short*:            shmem_thread_short_put,                \
             int*:              shmem_thread_int_put,                  \
             long*:             shmem_thread_long_put,                 \
             long long*:        shmem_thread_longlong_put              \
            )(dest, source, nelems, pe)

#define shmem_thread_p(dest, source, pe)                               \
    _Generic(&*(dest),                                          \
             float*:            shmem_thread_float_p,                  \
             double*:           shmem_thread_double_p,                 \
             long double*:      shmem_thread_longdouble_p,             \
             char*:             shmem_thread_char_p,                   \
             short*:            shmem_thread_short_p,                  \
             int*:              shmem_thread_int_p,                    \
             long*:             shmem_thread_long_p,                   \
             long long*:        shmem_thread_longlong_p                \
            )(dest, source, pe)

#define shmem_thread_iput(dest, source, dst, sst, nelems, pe)          \
    _Generic(&*(dest),                                          \
             float*:            shmem_thread_float_iput,               \
             double*:           shmem_thread_double_iput,              \
             long double*:      shmem_thread_longdouble_iput,          \
             char*:             shmem_thread_char_iput,                \
             short*:            shmem_thread_short_iput,               \
             int*:              shmem_thread_int_iput,                 \
             long*:             shmem_thread_long_iput,                \
             long long*:        shmem_thread_longlong_iput             \
            )(dest, source, dst, sst, nelems, pe)

/* Blocking block, scalar, and block-strided get */
#define shmem_thread_get(dest, source, nelems, pe)                     \
    _Generic(&*(dest),                                          \
             float*:            shmem_thread_float_get,                \
             double*:           shmem_thread_double_get,               \
             long double*:      shmem_thread_longdouble_get,           \
             char*:             shmem_thread_char_get,                 \
             short*:            shmem_thread_short_get,                \
             int*:              shmem_thread_int_get,                  \
             long*:             shmem_thread_long_get,                 \
             long long*:        shmem_thread_longlong_get              \
            )(dest, source, nelems, pe)

#define shmem_thread_g(dest, pe)                                       \
    _Generic(&*(dest),                                          \
             float*:            shmem_thread_float_g,                  \
             double*:           shmem_thread_double_g,                 \
             long double*:      shmem_thread_longdouble_g,             \
             char*:             shmem_thread_char_g,                   \
             short*:            shmem_thread_short_g,                  \
             int*:              shmem_thread_int_g,                    \
             long*:             shmem_thread_long_g,                   \
             long long*:        shmem_thread_longlong_g                \
            )(dest, pe)

#define shmem_thread_iget(dest, source, dst, sst, nelems, pe)          \
    _Generic(&*(dest),                                          \
             float*:            shmem_thread_float_iget,               \
             double*:           shmem_thread_double_iget,              \
             long double*:      shmem_thread_longdouble_iget,          \
             char*:             shmem_thread_char_iget,                \
             short*:            shmem_thread_short_iget,               \
             int*:              shmem_thread_int_iget,                 \
             long*:             shmem_thread_long_iget,                \
             long long*:        shmem_thread_longlong_iget             \
            )(dest, source, dst, sst, nelems, pe)

/* Nonblocking block put/get */
#define shmem_thread_put_nbi(dest, source, nelems, pe)                 \
    _Generic(&*(dest),                                          \
             float*:            shmem_thread_float_put_nbi,            \
             double*:           shmem_thread_double_put_nbi,           \
             long double*:      shmem_thread_longdouble_put_nbi,       \
             char*:             shmem_thread_char_put_nbi,             \
             short*:            shmem_thread_short_put_nbi,            \
             int*:              shmem_thread_int_put_nbi,              \
             long*:             shmem_thread_long_put_nbi,             \
             long long*:        shmem_thread_longlong_put_nbi          \
            )(dest, source, nelems, pe)

#define shmem_thread_get_nbi(dest, source, nelems, pe)                 \
    _Generic(&*(dest),                                          \
             float*:            shmem_thread_float_get_nbi,            \
             double*:           shmem_thread_double_get_nbi,           \
             long double*:      shmem_thread_longdouble_get_nbi,       \
             char*:             shmem_thread_char_get_nbi,             \
             short*:            shmem_thread_short_get_nbi,            \
             int*:              shmem_thread_int_get_nbi,              \
             long*:             shmem_thread_long_get_nbi,             \
             long long*:        shmem_thread_longlong_get_nbi          \
            )(dest, source, nelems, pe)

/* Atomics with standard AMO types */
#define shmem_thread_add(dest, value, pe)                              \
    _Generic(&*(dest),                                          \
             int*:              shmem_thread_int_add,                  \
             long*:             shmem_thread_long_add,                 \
             long long*:        shmem_thread_longlong_add              \
            )(dest, value, pe)

#define shmem_thread_cswap(dest, cond, value, pe)                      \
    _Generic(&*(dest),                                          \
             int*:              shmem_thread_int_cswap,                \
             long*:             shmem_thread_long_cswap,               \
             long long*:        shmem_thread_longlong_cswap            \
            )(dest, cond, value, pe)

#define shmem_thread_finc(dest, pe)                                    \
    _Generic(&*(dest),                                          \
             int*:              shmem_thread_int_finc,                 \
             long*:             shmem_thread_long_finc,                \
             long long*:        shmem_thread_longlong_finc             \
            )(dest, pe)

#define shmem_thread_inc(dest, pe)                                     \
    _Generic(&*(dest),                                          \
             int*:              shmem_thread_int_inc,                  \
             long*:             shmem_thread_long_inc,                 \
             long long*:        shmem_thread_longlong_inc              \
            )(dest, pe)

#define shmem_thread_fadd(dest, value, pe)                             \
    _Generic(&*(dest),                                          \
             int*:              shmem_thread_int_fadd,                 \
             long*:             shmem_thread_long_fadd,                \
             long long*:        shmem_thread_longlong_fadd             \
            )(dest, value, pe)

/* Atomics with extended AMO types */
#define shmem_thread_swap(dest, value, pe)                             \
    _Generic(&*(dest),                                          \
             float*:            shmem_thread_float_swap,               \
             double*:           shmem_thread_double_swap,              \
             int*:              shmem_thread_int_swap,                 \
             long*:             shmem_thread_long_swap,                \
             long long*:        shmem_thread_longlong_swap             \
            )(dest, value, pe)

#define shmem_thread_fetch(source, pe)                                 \
    _Generic(&*(source),                                        \
             float*:            shmem_thread_float_fetch,              \
             double*:           shmem_thread_double_fetch,             \
             int*:              shmem_thread_int_fetch,                \
             long*:             shmem_thread_long_fetch,               \
             long long*:        shmem_thread_longlong_fetch,           \
             const float*:      shmem_thread_float_fetch,              \
             const double*:     shmem_thread_double_fetch,             \
             const int*:        shmem_thread_int_fetch,                \
             const long*:       shmem_thread_long_fetch,               \
             const long long*:  shmem_thread_longlong_fetch            \
            )(source, pe)

#define shmem_thread_set(dest, value, pe)                              \
    _Generic(&*(dest),                                          \
             float*:            shmem_thread_float_set,                \
             double*:           shmem_thread_double_set,               \
             int*:              shmem_thread_int_set,                  \
             long*:             shmem_thread_long_set,                 \
             long long*:        shmem_thread_longlong_set              \
            )(dest, value, pe)
#else
long shmem_thread_swap(long *target, long value, int pe);
#endif /* C11 */

#ifndef SHMEM_INTERNAL_INCLUDE

#undef SHMEM_EVAL_MACRO_FOR_RMA
#undef SHMEM_EVAL_MACRO_FOR_AMO
#undef SHMEM_EVAL_MACRO_FOR_EXTENDED_AMO
#undef SHMEM_EVAL_MACRO_FOR_INTS
#undef SHMEM_EVAL_MACRO_FOR_FLOATS
#undef SHMEM_EVAL_MACRO_FOR_CMPLX
#undef SHMEM_EVAL_MACRO_FOR_SIZES

#undef SHMEM_DEFINE_FOR_RMA
#undef SHMEM_DEFINE_FOR_AMO
#undef SHMEM_DEFINE_FOR_EXTENDED_AMO
#undef SHMEM_DEFINE_FOR_INTS
#undef SHMEM_DEFINE_FOR_FLOATS
#undef SHMEM_DEFINE_FOR_CMPLX
#undef SHMEM_DEFINE_FOR_SIZES

#endif

#endif /* SHMEM_THREAD_H */

