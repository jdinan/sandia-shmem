#include <time.h>
#include <pthread.h>

#define SHMEM_HANG_WORKAROUND

void static inline bi_bw_ctx(int len, perf_metrics_t *metric_info)
{
    double start = 0.0, end = 0.0;
    int i = 0, j = 0;
    int dest = partner_node(*metric_info);

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

#ifdef SHMEM_HANG_WORKAROUND
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
    for (j = 0; j < metric_info->nthreads; j++) {
        shmemx_ctx_quiet(ctxs[j]);
    }
    shmem_barrier_all();

    for (int i = 0; i < metric_info->num_pes; i++) {
        assert(buf[i] == i);
    }
    shmem_free(buf);
#endif

    shmem_barrier_all();

#pragma omp parallel default(none) firstprivate(ctxs, metric_info, len, dest, \
        srcs, dests, stderr) private(j) shared(start) num_threads(metric_info->nthreads)
    {
        int i;
        const int thread_id = omp_get_thread_num();
        const shmemx_ctx_t ctx = ctxs[thread_id];
        char *src_buffer = srcs[thread_id];
        char *dest_buffer = dests[thread_id];

        for (i = 0; i < metric_info->trials + metric_info->warmup; i++) {
            if (i == metric_info->warmup) {
                shmemx_ctx_quiet(ctx);

#pragma omp barrier // Keep threads in sync

#pragma omp master
                {
                    start = perf_shmemx_wtime();
                }
            }

            for (j = 0; j < metric_info->window_size; j++) {
                shmemx_ctx_putmem(dest_buffer, src_buffer, len, dest, ctx);
            }
            shmemx_ctx_quiet(ctx);
        }
    }
    end = perf_shmemx_wtime();

    if (streaming_node(*metric_info)) {
        calc_and_print_results((end - start), len, *metric_info);
    }

    shmem_barrier_all();

    for (i = 0; i < metric_info->nthreads; i++) {
        shmemx_ctx_destroy(ctxs[i]);
        aligned_buffer_free(srcs[i]);
        aligned_buffer_free(dests[i]);
    }
    shmemx_domain_destroy(metric_info->nthreads, domains);

    free(ctxs);
    free(srcs);
    free(dests);
    free(domains);
}
