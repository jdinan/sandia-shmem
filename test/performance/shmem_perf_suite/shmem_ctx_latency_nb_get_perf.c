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
**  Notice: micro benchmark ~ two nodes only
**
**  Features of Test:
**  1) small get latency test
**  2) getmem latency test to calculate latency of various sizes
**
*/

#include <latency_common.h>
#include <pthread.h>

int main(int argc, char *argv[])
{

    latency_main(argc, argv, 1);

    return 0;
}  /* end of main() */

void
long_element_round_trip_latency(perf_metrics_t data) { }

void
int_element_latency(perf_metrics_t data) { }

typedef struct _streaming_driver_data {
    perf_metrics_t *data;
    pthread_barrier_t *barrier;
    int thread_id;
    shmemx_ctx_t ctx;
    int len;
    double *start;
} streaming_driver_data;

static void *streaming_driver(void *user_data) {
    int i;
    streaming_driver_data *thread_data = (streaming_driver_data *)user_data;
    perf_metrics_t *data = thread_data->data;
    pthread_barrier_t *barrier = thread_data->barrier;
    const int thread_id = thread_data->thread_id;
    const shmemx_ctx_t ctx = thread_data->ctx;
    const int len = thread_data->len;

    for (i = 0; i < data->trials + data->warmup; i++) {
        if (i == data->warmup) {
            shmemx_ctx_quiet(ctx);

            pthread_barrier_wait(barrier);

            if (thread_id == 0) {
                *(thread_data->start) = perf_shmemx_wtime();
            }
        }

        shmemx_ctx_getmem_nbi(data->dest, data->src, len, 1, ctx);
        shmemx_ctx_quiet(ctx);
    }

    pthread_barrier_wait(barrier);

    return NULL;
}

void
streaming_latency(int len, perf_metrics_t *data)
{
    double start = 0.0;
    double end = 0.0;
    static int print_once = 0;
    if(!print_once && data->my_node == GET_IO_NODE) {
        printf("\nStreaming results for %d trials each of length %d through %d in"\
              " powers of %d\n", data->trials, data->start_len,
              data->max_len, data->inc);
        print_results_header();
        print_once++;
    }

    if (data->my_node == 0) {
        int i;
        pthread_barrier_t barrier;
        pthread_t *threads = (pthread_t *)malloc(
                (data->nthreads - 1) * sizeof(*threads));
        streaming_driver_data *thread_data = (streaming_driver_data *)malloc(
                data->nthreads * sizeof(*thread_data));
        assert(threads && thread_data);
        int err = pthread_barrier_init(&barrier, NULL, data->nthreads);
        assert(err == 0);

        for (i = 0; i < data->nthreads; i++) {
            thread_data[i].data = data;
            thread_data[i].barrier = &barrier;
            thread_data[i].thread_id = i;
            thread_data[i].ctx = data->ctxs[i];
            thread_data[i].len = len;
            thread_data[i].start = &start;
        }

        for (i = 0; i < data->nthreads - 1; i++) {
            err = pthread_create(&threads[i], NULL, streaming_driver,
                    thread_data + (i + 1));
            assert(err == 0);
        }

        streaming_driver(thread_data);

        end = perf_shmemx_wtime();

        for (i = 0; i < data->nthreads - 1; i++) {
            err = pthread_join(threads[i], NULL);
            assert(err == 0);
        }

        calc_and_print_results(start, end, len, *data, 1);

        free(threads);
        free(thread_data);
        pthread_barrier_destroy(&barrier);
    }
} /* latency/bw for one-way trip */
