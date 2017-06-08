#include <pthread.h>

typedef struct _get_driver_data {
    perf_metrics_t *data;
    shmemx_ctx_t ctx;
    int dest;
    pthread_barrier_t *barrier;
    double start, end;
} get_driver_data;

static void *long_element_round_trip_latency_get_driver(void *user_data) {
    int i;
    get_driver_data *thread_data = (get_driver_data *)user_data;

    const shmemx_ctx_t ctx = thread_data->ctx;
    perf_metrics_t *data = thread_data->data;
    const int dest = thread_data->dest;
    pthread_barrier_t *barrier = thread_data->barrier;

    for (i = 0; i < data->trials + data->warmup; i++) {
        if(i == data->warmup) {
            shmemx_ctx_quiet(ctx);

            pthread_barrier_wait(barrier);

            thread_data->start = perf_shmemx_wtime();
        }

        *(data->target) = shmemx_ctx_long_g(data->target, dest, ctx);
    }

    thread_data->end = perf_shmemx_wtime();

    return NULL;
}

void static inline
long_element_round_trip_latency_get_ctx(perf_metrics_t data)
{
    int i;
    int dest = 1;
    int partner_pe = partner_node(data.my_node);
    *data.target = data.my_node;

    if (data.my_node == GET_IO_NODE) {
        printf("\nshmem_long_g results:\n");
        print_results_header();
    }

    pthread_barrier_t barrier;
    int err = pthread_barrier_init(&barrier, NULL, data.nthreads);
    assert(err == 0);
    pthread_t *threads = (pthread_t *)malloc(
            (data.nthreads - 1) * sizeof(*threads));
    get_driver_data *thread_data = (get_driver_data *)malloc(
            data.nthreads * sizeof(*thread_data));
    assert(threads && thread_data);

    for (i = 0; i < data.nthreads; i++) {
        thread_data[i].data = &data;
        thread_data[i].ctx = data.ctxs[i];
        thread_data[i].dest = dest;
        thread_data[i].barrier = &barrier;
    }

    shmem_barrier_all();

    if (data.my_node == GET_IO_NODE) {
        for (i = 1; i < data.nthreads; i++) {
            err = pthread_create(&threads[i - 1], NULL,
                    long_element_round_trip_latency_get_driver,
                    thread_data + i);
            assert(err == 0);
        }

        long_element_round_trip_latency_get_driver(thread_data);

        double sum_cpu_time = thread_data[0].end - thread_data[0].start;

        for (i = 1; i < data.nthreads; i++) {
            err = pthread_join(threads[i - 1], NULL);
            assert(err == 0);
            sum_cpu_time += thread_data[i].end - thread_data[i].start;
        }

        calc_and_print_results(sum_cpu_time, sizeof(long), data, 1);

        if(data.validate) {
            if(*data.target != partner_pe)
                printf("validation error shmem_long_g target = %ld != %d\n",
                        *data.target, partner_pe);
        }
    }

    free(threads);
    free(thread_data);
    pthread_barrier_destroy(&barrier);
} /*gauge small get pathway round trip latency*/

typedef struct _put_driver_data {
    perf_metrics_t *data;
    shmemx_ctx_t ctx;
    int dest;
    long *my_target;
    long *my_tmp;
    pthread_barrier_t *barrier;
    double start, end;
} put_driver_data;

static void *long_element_round_trip_latency_put_driver(void *user_data) {
    int i;
    put_driver_data *thread_data = (put_driver_data *)user_data;

    const shmemx_ctx_t ctx = thread_data->ctx;
    perf_metrics_t *data = thread_data->data;
    const int dest = thread_data->dest;
    long *my_target = thread_data->my_target;
    long *my_tmp = thread_data->my_tmp;
    pthread_barrier_t *barrier = thread_data->barrier;

    for (i = 0; i < data->trials + data->warmup; i++) {
        if(i == data->warmup) {
            shmemx_ctx_quiet(ctx);

            pthread_barrier_wait(barrier);

            thread_data->start = perf_shmemx_wtime();
        }

        shmemx_ctx_long_p(my_target, ++(*my_tmp), dest, ctx);

        shmem_long_wait_until(my_target, SHMEM_CMP_EQ, *my_tmp);
    }

    thread_data->end = perf_shmemx_wtime();

    return NULL;
}

static void *long_element_round_trip_latency_put_receiver(void *user_data) {
    int i;
    put_driver_data *thread_data = (put_driver_data *)user_data;

    const shmemx_ctx_t ctx = thread_data->ctx;
    perf_metrics_t *data = thread_data->data;
    const int dest = thread_data->dest;
    long *my_target = thread_data->my_target;
    long *my_tmp = thread_data->my_tmp;
    pthread_barrier_t *barrier = thread_data->barrier;

    for (i = 0; i < data->trials + data->warmup; i++) {
        shmem_long_wait_until(my_target, SHMEM_CMP_EQ, ++(*my_tmp));

        shmemx_ctx_long_p(my_target, *my_tmp, dest, ctx);
    }

    pthread_barrier_wait(barrier);

    return NULL;
}

void static inline
long_element_round_trip_latency_put_ctx(perf_metrics_t data)
{
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

    pthread_barrier_t barrier;
    int err = pthread_barrier_init(&barrier, NULL, data.nthreads);
    assert(err == 0);
    pthread_t *threads = (pthread_t *)malloc(
            (data.nthreads - 1) * sizeof(*threads));
    put_driver_data *thread_data = (put_driver_data *)malloc(
            data.nthreads * sizeof(*thread_data));
    assert(threads && thread_data);

    for (i = 0; i < data.nthreads; i++) {
        thread_data[i].data = &data;
        thread_data[i].ctx = data.ctxs[i];
        thread_data[i].dest = dest;
        thread_data[i].my_target = targets + i;
        thread_data[i].my_tmp = tmps + i;
        thread_data[i].barrier = &barrier;
    }

    shmem_barrier_all();

    if (data.my_node == PUT_IO_NODE) {
        for (i = 1; i < data.nthreads; i++) {
            err = pthread_create(&threads[i - 1], NULL,
                    long_element_round_trip_latency_put_driver,
                    thread_data + i);
            assert(err == 0);
        }

        long_element_round_trip_latency_put_driver(thread_data);

        double sum_cpu_time = thread_data[0].end - thread_data[0].start;

        for (i = 1; i < data.nthreads; i++) {
            err = pthread_join(threads[i - 1], NULL);
            assert(err == 0);
            sum_cpu_time += thread_data[i].end - thread_data[i].start;
        }

        data.trials = data.trials*2; /*output half to get single round trip time*/
        calc_and_print_results(sum_cpu_time, sizeof(long), data, 1);
   } else {
        for (i = 1; i < data.nthreads; i++) {
            err = pthread_create(&threads[i - 1], NULL,
                    long_element_round_trip_latency_put_receiver,
                    thread_data + i);
            assert(err == 0);
        }

        long_element_round_trip_latency_put_receiver(thread_data);

        for (i = 1; i < data.nthreads; i++) {
            err = pthread_join(threads[i - 1], NULL);
            assert(err == 0);
        }
   }

    free(tmps);
    shmem_free(targets);
} /*gauge small put pathway round trip latency*/
