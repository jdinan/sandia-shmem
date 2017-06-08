#include <pthread.h>

typedef struct _driver_data {
    int tid;
    shmemx_ctx_t ctx;
    perf_metrics_t *data;
    double *start;
    pthread_barrier_t *barrier;
} driver_data;

static void *int_p_latency_driver(void *user_data) {
    int i;
    driver_data *thread_data = (driver_data *)user_data;

    const int thread_id = thread_data->tid;
    const shmemx_ctx_t ctx = thread_data->ctx;
    perf_metrics_t *data = thread_data->data;
    pthread_barrier_t *barrier = thread_data->barrier;

    for (i = 0; i < data->trials + data->warmup; i++) {
        if(i == data->warmup) {
            shmemx_ctx_quiet(ctx);

            pthread_barrier_wait(barrier);

            if (thread_id == 0) {
                *(thread_data->start) = perf_shmemx_wtime();
            }
        }

        shmemx_ctx_int_p((int*) data->dest, data->my_node, 0, ctx);
        shmemx_ctx_quiet(ctx);
    }

    pthread_barrier_wait(barrier);
    
    return NULL;
}

void static inline
int_p_latency_ctx(perf_metrics_t data)
{
    double start = 0.0;
    double end = 0.0;

    if (data.my_node == PUT_IO_NODE) {
        printf("\nStream shmem_int_p results:\n");
        print_results_header();
    }

    pthread_barrier_t barrier;
    int err = pthread_barrier_init(&barrier, NULL, data.nthreads);
    assert(err == 0);
    pthread_t *threads = (pthread_t *)malloc(
            (data.nthreads - 1) * sizeof(*threads));
    driver_data *thread_data = (driver_data *)malloc(
            data.nthreads * sizeof(*thread_data));
    assert(threads && thread_data);

    for (int i = 0; i < data.nthreads; i++) {
        thread_data[i].tid = i;
        thread_data[i].ctx = data.ctxs[i];
        thread_data[i].data = &data;
        thread_data[i].start = &start;
        thread_data[i].barrier = &barrier;
    }

    /*puts to zero to match gets validation scheme*/
    if (data.my_node == PUT_IO_NODE) {
        int i;
        for (i = 1; i < data.nthreads; i++) {
            err = pthread_create(&threads[i - 1], NULL, int_p_latency_driver,
                    thread_data + i);
            assert(err == 0);
        }

        int_p_latency_driver(thread_data);

        end = perf_shmemx_wtime();

        for (i = 1; i < data.nthreads; i++) {
            err = pthread_join(threads[i - 1], NULL);
            assert(err == 0);
        }

        calc_and_print_results(start, end, sizeof(int), data, 1);
    }

    shmem_barrier_all();

    pthread_barrier_destroy(&barrier);
    free(threads);
    free(thread_data);

    if((data.my_node == 0) && data.validate)
        validate_recv(data.dest, sizeof(int), partner_node(data.my_node));

} /* latency/bw for one-way trip */

static void *int_g_latency_driver(void *user_data) {
    int i;
    driver_data *thread_data = (driver_data *)user_data;

    const int thread_id = thread_data->tid;
    const shmemx_ctx_t ctx = thread_data->ctx;
    perf_metrics_t *data = thread_data->data;
    pthread_barrier_t *barrier = thread_data->barrier;

    for (i = 0; i < data->trials + data->warmup; i++) {
        if(i == data->warmup) {
            shmemx_ctx_quiet(ctx);

            pthread_barrier_wait(barrier);

            if (thread_id == 0) {
                *(thread_data->start) = perf_shmemx_wtime();
            }
        }

        int rtnd = shmemx_ctx_int_g((int*) data->src, 1, ctx);
    }

    pthread_barrier_wait(barrier);
    
    return NULL;
}

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

    pthread_barrier_t barrier;
    int err = pthread_barrier_init(&barrier, NULL, data.nthreads);
    assert(err == 0);
    pthread_t *threads = (pthread_t *)malloc(
            (data.nthreads - 1) * sizeof(*threads));
    driver_data *thread_data = (driver_data *)malloc(
            data.nthreads * sizeof(*thread_data));
    assert(threads && thread_data);

    for (int i = 0; i < data.nthreads; i++) {
        thread_data[i].tid = i;
        thread_data[i].ctx = data.ctxs[i];
        thread_data[i].data = &data;
        thread_data[i].start = &start;
        thread_data[i].barrier = &barrier;
    }

    if (data.my_node == GET_IO_NODE) {
        int i;
        for (i = 1; i < data.nthreads; i++) {
            err = pthread_create(&threads[i - 1], NULL, int_g_latency_driver,
                    thread_data + i);
            assert(err == 0);
        }

        int_g_latency_driver(thread_data);

        end = perf_shmemx_wtime();

        for (i = 1; i < data.nthreads; i++) {
            err = pthread_join(threads[i - 1], NULL);
            assert(err == 0);
        }

        calc_and_print_results(start, end, sizeof(int), data, 1);
    }

    shmem_barrier_all();

    // if((data.my_node == 0) && data.validate)
    //     validate_recv((char*) &rtnd, sizeof(int), partner_node(data.my_node));
}
