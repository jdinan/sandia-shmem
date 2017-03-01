#include <time.h>
#include <pthread.h>

typedef struct _thread_params {
    unsigned tid;
    int len;
    perf_metrics_t *metric_info;
    int streaming_node;
    shmemx_ctx_t ctx;
    char *src_buffer, *dest_buffer;
} thread_params;

#define SHMEM_HANG_WORKAROUND

static pthread_barrier_t barrier;

static void *helper(void *user_data) {
    thread_params *params = (thread_params *)user_data;
    if (params->tid == 0) {
        shmem_barrier_all();
    }

    int err = pthread_barrier_wait(&barrier);
    assert(err == 0 || PTHREAD_BARRIER_SERIAL_THREAD);

    if (streaming_node) {
        double start, end;
        const int thread_id = params->tid;
        const shmemx_ctx_t ctx = params->ctx;
        char *src_buffer = params->src_buffer;
        char *dest_buffer = params->dst_buffer;

        int i;

        for (i = 0; i < metric_info->trials + metric_info->warmup; i++) {
            if (i == metric_info->warmup) {
                shmemx_ctx_quiet(ctx);

                err = pthread_barrier_wait(&barrier);
                assert(err == 0 || PTHREAD_BARRIER_SERIAL_THREAD);

                start = perf_shmemx_wtime();
            }

            for (j = 0; j < metric_info->window_size; j++) {
                shmemx_ctx_putmem(dest_buffer, src_buffer, len, dest, ctx);
            }
            shmemx_ctx_quiet(ctx);
        }

        end = perf_shmemx_wtime();

        if (params->tid == 0) {
            calc_and_print_results(end - start, len, *(params->metric_info));
        }
    }

    return NULL;
}

void static inline uni_bw_ctx(int len, perf_metrics_t *metric_info,
        int streaming_node)
{

#ifdef SHMEM_HANG_WORKAROUND
#endif

    double start = 0.0, end = 0.0;
    int i = 0, j = 0;
    int dest = partner_node(*metric_info);

    // Set up domains and contexts, one context per thread
    shmemx_domain_t *domains = (shmemx_ctx_t *)malloc(
            metric_info->nthreads * sizeof(shmemx_domain_t));
    shmemx_ctx_t *ctxs = (shmemx_ctx_t *)malloc(
            metric_info->nthreads * sizeof(shmemx_ctx_t));
    assert(domains && ctxs);
    shmemx_domain_create(metric_info->thread_safety, metric_info->nthreads, domains);

    char **srcs = (char **)malloc(metric_info->nthreads * sizeof(char *));
    char **dests = (char **)malloc(metric_info->nthreads * sizeof(char *));
    assert(srcs && dests);

    for (i = 0; i < metric_info->nthreads; i++) {
        shmemx_ctx_create(domains[i], ctxs + i);
        srcs[i] = aligned_buffer_alloc(len);
        dests[i] = aligned_buffer_alloc(len);
        assert(srcs[i] && dests[i]);
    }

    int err = pthread_barrier_init(&barrier, NULL, metric_info->nthreads);
    assert(err == 0);

    int *buf = (int *)shmem_malloc(metric_info->num_pes * sizeof(int));
    assert(buf);
    buf[metric_info->my_node] = metric_info->my_node;

    for (i = 0; i < metric_info->num_pes; i++) {
        if (i == metric_info->my_node) continue;

        int j;
        for (j = 0; j < metric_info->nthreads; j++) {
            shmemx_ctx_putmem(buf + metric_info->my_node,
                    buf + metric_info->my_node, sizeof(int), i, ctxs[j]);
        }
    }
    shmem_barrier_all();

    for (int i = 0; i < metric_info->num_pes; i++) {
        assert(buf[i] == i);
    }

    pthread_t *threads = (pthread_t *)malloc(
            metric_info->nthreads * sizeof(pthread_t));
    assert(threads);
    thread_params *params = (thread_params *)malloc(
            metric_info->nthreads * sizeof(thread_params));
    assert(params);

    for (i = 0; i < metric_info->nthreads; i++) {
        params[i].tid = i;
        params[i].len = len;
        params[i].metric_info = metric_info;
        params[i].streaming_node = streaming_node;
        params[i].ctx = ctxs[i];
        params[i].src_buffer = srcs[i];
        params[i].dest_buffer = dests[i];
    }

    for (i = 1; i < metric_info->nthreads; i++) {
        pthread_create(&threads[i], NULL, helper, params + i);
    }
    helper(params + 0);

//     shmem_barrier_all();
// 
//     if (streaming_node) {
// 
// #pragma omp parallel default(none) firstprivate(ctxs, metric_info, len, dest, \
//         srcs, dests) private(j) shared(start)
//         {
//             int i;
//             const int thread_id = omp_get_thread_num();
//             const shmemx_ctx_t ctx = ctxs[thread_id];
//             char *src_buffer = srcs[thread_id];
//             char *dest_buffer = dests[thread_id];
// 
//             for (i = 0; i < metric_info->trials + metric_info->warmup; i++) {
//                 if (i == metric_info->warmup) {
//                     shmemx_ctx_quiet(ctx);
// 
// #pragma omp barrier // Keep threads in sync
//                     if (thread_id == 0) {
//                         start = perf_shmemx_wtime();
//                     }
//                 }
// 
//                 for (j = 0; j < metric_info->window_size; j++) {
//                     shmemx_ctx_putmem(dest_buffer, src_buffer, len, dest, ctx);
//                 }
//                 shmemx_ctx_quiet(ctx);
//             }
//         }
//         end = perf_shmemx_wtime();
// 
//         calc_and_print_results((end - start), len, *metric_info);
//     }

    for (i = 1; i < metric_info->nthreads; i++) {
        pthread_join(threads[i], NULL);
    }

    shmem_barrier_all();

    for (i = 0; i < metric_info->nthreads; i++) {
        shmemx_ctx_destroy(ctxs[i]);
        shmem_free(srcs[i]);
        shmem_free(dests[i]);
    }
    shmemx_domain_destroy(metric_info->nthreads, domains);

    free(ctxs);
    free(srcs);
    free(dests);
    free(domains);
    free(threads);
    free(params);
}
