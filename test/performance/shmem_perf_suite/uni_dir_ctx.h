#include <time.h>
#include <pthread.h>

typedef struct _driver_data {
    int tid;
    shmemx_ctx_t ctx;
    char *src_buffer;
    char *dest_buffer;
    pthread_barrier_t *barrier;
    double *start;
    perf_metrics_t *metric_info;
    int len;
} __attribute__ ((aligned (128))) driver_data;

static void *driver(void *user_data) {
    int i, j;

    driver_data *data = (driver_data *)user_data;

    const int thread_id = data->tid;
    const shmemx_ctx_t ctx = data->ctx;
    char *src_buffer = data->src_buffer;
    char *dest_buffer = data->dest_buffer;
    pthread_barrier_t *barrier = data->barrier;
    perf_metrics_t *metric_info = data->metric_info;
    const int len = data->len;
    const int dest = partner_node(*metric_info);

    for (i = 0; i < metric_info->trials + metric_info->warmup; i++) {
        if (i == metric_info->warmup) {
            shmemx_ctx_quiet(ctx);

            pthread_barrier_wait(barrier);

            if (thread_id == 0) {
                *(data->start) = perf_shmemx_wtime();
            }
        }

        for (j = 0; j < metric_info->window_size; j++) {
            shmemx_ctx_putmem(dest_buffer, src_buffer, len, dest, ctx);
        }
        shmemx_ctx_quiet(ctx);
    }

    pthread_barrier_wait(barrier);

    return NULL;
}

void static inline uni_bw_ctx(int len, perf_metrics_t *metric_info,
        int streaming_node)
{
    double start = 0.0, end = 0.0;
    int i = 0;

    // Set up domains and contexts, one context per thread
    shmemx_domain_t *domains = (shmemx_ctx_t *)malloc(
            metric_info->nthreads * sizeof(shmemx_domain_t));
    shmemx_ctx_t *ctxs = (shmemx_ctx_t *)malloc(
            metric_info->nthreads * sizeof(shmemx_ctx_t));
    assert(domains && ctxs);
    shmemx_domain_create(metric_info->domain_thread_safety,
            metric_info->nthreads, domains);

    char **srcs = (char **)malloc(metric_info->nthreads * sizeof(char *));
    char **dests = (char **)malloc(metric_info->nthreads * sizeof(char *));
    assert(srcs && dests);

    for (i = 0; i < metric_info->nthreads; i++) {
        shmemx_ctx_create(domains[i], ctxs + i);
        srcs[i] = aligned_buffer_alloc(len);
        dests[i] = aligned_buffer_alloc(len);
        assert(srcs[i] && dests[i]);
    }

    pthread_barrier_t barrier;
    int err = pthread_barrier_init(&barrier, NULL, metric_info->nthreads);
    assert(err == 0);
    pthread_t *threads = (pthread_t *)malloc(
            (metric_info->nthreads - 1) * sizeof(*threads));
    driver_data *thread_data = (driver_data *)malloc(
            metric_info->nthreads * sizeof(*thread_data));
    assert(threads && thread_data);

    for (i = 0; i < metric_info->nthreads; i++) {
        thread_data[i].tid = i;
        thread_data[i].ctx = ctxs[i];
        thread_data[i].src_buffer = srcs[i];
        thread_data[i].dest_buffer = dests[i];
        thread_data[i].barrier = &barrier;
        thread_data[i].start = &start;
        thread_data[i].metric_info = metric_info;
        thread_data[i].len = len;
    }

    shmem_barrier_all();

    if (streaming_node) {
        for (i = 1; i < metric_info->nthreads; i++) {
            err = pthread_create(&threads[i - 1], NULL, driver, thread_data + i);
            assert(err == 0);
        }

        driver(thread_data);

        end = perf_shmemx_wtime();

        for (i = 1; i < metric_info->nthreads; i++) {
            err = pthread_join(threads[i - 1], NULL);
            assert(err == 0);
        }

        calc_and_print_results((end - start), len, *metric_info);
    }

    shmem_barrier_all();

    for (i = 0; i < metric_info->nthreads; i++) {
        shmemx_ctx_destroy(ctxs[i]);
        aligned_buffer_free(srcs[i]);
        aligned_buffer_free(dests[i]);
    }
    shmemx_domain_destroy(metric_info->nthreads, domains);

    pthread_barrier_destroy(&barrier);

    free(threads);
    free(thread_data);
    free(ctxs);
    free(srcs);
    free(dests);
    free(domains);
}
