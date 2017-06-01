#include <omp.h>

void static inline
long_element_round_trip_latency_get_ctx(perf_metrics_t data)
{
    double start = 0.0;
    double end = 0.0;
    int dest = 1;
    int partner_pe = partner_node(data.my_node);
    *data.target = data.my_node;

    if (data.my_node == GET_IO_NODE) {
        printf("\nshmem_long_g results:\n");
        print_results_header();
    }

    shmem_barrier_all();

    if (data.my_node == GET_IO_NODE) {

#pragma omp parallel default(none) num_threads(data.nthreads) \
        firstprivate(data, dest) shared(start)
        {
            int i;
            const int thread_id = omp_get_thread_num();
            const shmemx_ctx_t ctx = data.ctxs[thread_id];

            for (i = 0; i < data.trials + data.warmup; i++) {
                if(i == data.warmup) {
                    shmemx_ctx_quiet(ctx);

#pragma omp barrier

#pragma omp master
                    {
                        start = perf_shmemx_wtime();
                    }
                }

                *data.target = shmemx_ctx_long_g(data.target, dest, ctx);
            }
        }
        end = perf_shmemx_wtime();

        calc_and_print_results(start, end, sizeof(long), data);

        if(data.validate) {
            if(*data.target != partner_pe)
                printf("validation error shmem_long_g target = %ld != %d\n",
                        *data.target, partner_pe);
        }
    }
} /*gauge small get pathway round trip latency*/

void static inline
long_element_round_trip_latency_put_ctx(perf_metrics_t data)
{
    double start = 0.0;
    double end = 0.0;
    int dest = (data.my_node + 1) % data.npes, i = 0;

    long *tmps = (long *)malloc(data.nthreads * sizeof(long));
    long *targets = (long *)shmem_malloc(data.nthreads * sizeof(long));
    assert(targets && tmps);
    for (i = 0; i < data.nthreads; i++) {
        tmps[i] = targets[i] = INIT_VALUE;
    }


    if (data.my_node == PUT_IO_NODE) {
        printf("\nPing-Pong shmem_long_p results:\n");
        print_results_header();
    }

    shmem_barrier_all();

    if (data.my_node == PUT_IO_NODE) {

#pragma omp parallel default(none) num_threads(data.nthreads) \
        firstprivate(data, targets, tmps, dest) shared(start)
        {
            int i;
            const int thread_id = omp_get_thread_num();
            const shmemx_ctx_t ctx = data.ctxs[thread_id];
            long *my_target = targets + thread_id;
            long *my_tmp = tmps + thread_id;

            for (i = 0; i < data.trials + data.warmup; i++) {
                if(i == data.warmup) {
                    shmemx_ctx_quiet(ctx);

#pragma omp barrier

#pragma omp master
                    {
                        start = perf_shmemx_wtime();
                    }
                }

                shmemx_ctx_long_p(my_target, ++(*my_tmp), dest, ctx);

                shmem_long_wait_until(my_target, SHMEM_CMP_EQ, *my_tmp);
            }
        }
        end = perf_shmemx_wtime();

        data.trials = data.trials*2; /*output half to get single round trip time*/
        calc_and_print_results(start, end, sizeof(long), data);

   } else {
#pragma omp parallel default(none) num_threads(data.nthreads) \
        firstprivate(data, targets, tmps, dest) shared(start)
        {
            int i;
            const int thread_id = omp_get_thread_num();
            long *my_target = targets + thread_id;
            long *my_tmp = tmps + thread_id;

            for (i = 0; i < data.trials + data.warmup; i++) {
                shmem_long_wait_until(my_target, SHMEM_CMP_EQ, ++(*my_tmp));

                shmem_long_p(my_target, *my_tmp, dest);
            }
        }
   }

} /*gauge small put pathway round trip latency*/
