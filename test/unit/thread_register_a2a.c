/*
 *  Copyright (c) 2016 Intel Corporation. All rights reserved.
 *  This software is available to you under the BSD license below:
 *
 *      Redistribution and use in source and binary forms, with or
 *      without modification, are permitted provided that the following
 *      conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Multithreaded All-to-All Test Using Thread Registration
 * James Dinan <james.dinan@intel.com>
 * September, 2016
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <shmem.h>
#include <shmemx.h>

#include "shmem-thread.h"

#define T 1

int dest[T] = { 0 };
int flag[T] = { 0 };

int me, npes;
_Atomic int errors = 0;


static void * thread_main(void *arg) {
    int* flagvals = calloc(sizeof(int),npes);
    int* destvals = calloc(sizeof(int),npes);
    int tid = * (int *) arg;
    int i, val, expected;

    shmem_thread_register();

    /* TEST CONCURRENT ATOMICS */
    val = me;
    for (i = 1; i <= npes; i++)
        shmem_int_add(&dest[tid], val, (me + i) % npes);

    /* Ensure that fence does not overlap with communication calls */
    shmem_thread_barrier();
    if (tid == 0) shmem_fence();
    shmem_thread_barrier();

    for (i = 1; i <= npes; i++)
        shmem_int_inc(&flag[tid], (me + i) % npes);


    for (i = 1; i <= npes; i++) {
      shmem_int_wait_until(&flag[tid], SHMEM_CMP_GE, i);
      flagvals[i-1] = flag[tid];
      destvals[i-1] = dest[tid];
    }

    expected = (npes-1) * npes / 2;
    if (dest[tid] != expected || flag[tid] != npes) {
        printf("Atomic test error: [PE = %d | TID = %d] -- "
               "dest = %d (expected %d), flag = %d (expected %d)\n",
               me, tid, dest[tid], expected, flag[tid], npes);
        for (i = 1; i <= npes; i++) {
          printf("[%d,%d] flag = %d, dest = %d\n",me,tid,flagvals[i-1],destvals[i-1]);
        }
        ++errors;
    }

    shmem_thread_barrier();
    if (0 == tid) shmem_barrier_all();
    shmem_thread_barrier();

    /* TEST CONCURRENT PUTS */
    val = -1;
    shmem_int_put(&dest[tid], &val, 1, (me + 1) % npes);

    /* Ensure that all puts are issued before the shmem barrier is called. */
    shmem_thread_barrier();
    if (0 == tid) shmem_barrier_all();
    shmem_thread_barrier();

    /* TEST CONCURRENT GETS */
    for (i = 1; i <= npes; i++) {
        shmem_int_get(&val, &dest[tid], 1, (me + i) % npes);

        expected = -1;
        if (val != expected) {
            printf("Put/get test error: [PE = %d | TID = %d] -- From PE %d, got %d expected %d\n",
               me, tid, (me + i) % npes, val, expected);
            ++errors;
        }
    }

    shmem_thread_barrier();
    shmem_thread_unregister();

    return NULL;
}


int main(int argc, char **argv) {
    int tl, i;
    pthread_t threads[T];
    int t_arg[T];

    shmemx_init_thread(SHMEMX_THREAD_MULTIPLE, &tl);

    /* If OpenSHMEM doesn't support multithreading, exit gracefully */
    if (SHMEMX_THREAD_MULTIPLE != tl) {
        printf("Warning: Exiting because threading is disabled, tested nothing\n");
        shmem_finalize();
        return 0;
    }

    me = shmem_my_pe();
    npes = shmem_n_pes();

    if (me == 0) printf("Starting multithreaded test on %d PEs, %d threads/PE\n", npes, T);

    for (i = 0; i < T; i++) {
        int err;
        t_arg[i] = i;
        err = pthread_create(&threads[i], NULL, thread_main, (void*) &t_arg[i]);
        assert(0 == err);
    }

    for (i = 0; i < T; i++) {
        int err;
        err = pthread_join(threads[i], NULL);
        assert(0 == err);
    }

    if (me == 0) {
        if (errors) printf("Encountered %d errors\n", errors);
        else printf("Success\n");
    }

    shmem_finalize();
    return (errors == 0) ? 0 : 1;
}
