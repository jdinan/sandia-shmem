/*
 *  Copyright (c) 2015 Intel Corporation. All rights reserved.
 *  This software is available to you under the BSD license below:
 *
 * *	Redistribution and use in source and binary forms, with or
 *	without modification, are permitted provided that the following
 *	conditions are met:
 *
 *	- Redistributions of source code must retain the above
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
**
**  This is a bandwidth centric test for put: back-to-back message rate
**
**  Features of Test: uni-directional bandwidth
**
**  -by default megabytes/second results
**
**NOTE: this test assumes correctness of reduction algorithm
*/

#define ENABLE_OPENMP

#include <bw_common.h>

#define shmem_putmem(dest, source, nelems, pe) \
        shmem_putmem_nbi(dest, source, nelems, pe)

#include <uni_dir_ctx.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>


void sig_handler(int signo) {
    raise(SIGABRT);
    assert(0); // should never reach here
}

void *kill_func(void *data) {
    int kill_seconds = *((int *)data);

    double start_time_us = perf_shmemx_wtime();

    while (perf_shmemx_wtime() - start_time_us < kill_seconds * 1000000.0) {
        sleep(10);
    }
    // int err = sleep(kill_seconds);
    // assert(err == 0);
    raise(SIGUSR1);
    return NULL;
}

int main(int argc, char *argv[])
{

    int kill_seconds = 550;
    __sighandler_t serr = signal(SIGUSR1, sig_handler);
    assert(serr != SIG_ERR);

    pthread_t thread;
    const int perr = pthread_create(&thread, NULL, kill_func,
            (void *)&kill_seconds);
    assert(perr == 0);

    uni_dir_bw_main(argc, argv);

  fflush(stderr);
  fflush(stdout);

  shmem_finalize();

  return 0;
}

void
uni_dir_bw(int len, perf_metrics_t *metric_info)
{
    uni_bw_ctx(len, metric_info, !streaming_node(*metric_info));
}
