
void static inline
int_p_latency_ctx(perf_metrics_t data)
{
    double start = 0.0;
    double end = 0.0;

    if (data.my_node == PUT_IO_NODE) {
        printf("\nStream shmem_int_p results:\n");
        print_results_header();
    }

    /*puts to zero to match gets validation scheme*/
    if (data.my_node == PUT_IO_NODE) {

#pragma omp parallel default(none) firstprivate(data) shared(start) \
        num_threads(data.nthreads)
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

                shmemx_ctx_int_p((int*) data.dest, data.my_node, 0, ctx);
                shmemx_ctx_quiet(ctx);
            }
        }
        end = perf_shmemx_wtime();

        calc_and_print_results(start, end, sizeof(int), data);
    }

    shmem_barrier_all();

    if((data.my_node == 0) && data.validate)
        validate_recv(data.dest, sizeof(int), partner_node(data.my_node));

} /* latency/bw for one-way trip */

void static inline
int_g_latency_ctx(perf_metrics_t data)
{
    double start = 0.0;
    double end = 0.0;
    // int rtnd = -1;

    if (data.my_node == GET_IO_NODE) {
        printf("\nStream shmem_int_g results:\n");
        print_results_header();
    }

    if (data.my_node == GET_IO_NODE) {

#pragma omp parallel default(none) firstprivate(data) shared(start) \
        num_threads(data.nthreads)
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

                int rtnd = shmemx_ctx_int_g((int*) data.src, 1, ctx);
            }
        }
        end = perf_shmemx_wtime();

        calc_and_print_results(start, end, sizeof(int), data);
    }

    shmem_barrier_all();

    // if((data.my_node == 0) && data.validate)
    //     validate_recv((char*) &rtnd, sizeof(int), partner_node(data.my_node));
}
