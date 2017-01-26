#include <time.h>
#include <omp.h>

void static inline uni_bw_ctx(int len, perf_metrics_t *metric_info,
        int streaming_node)
{
    int nthreads;
#pragma omp parallel
#pragma omp master
    {
        nthreads = omp_get_num_threads();
    }

    double start = 0.0, end = 0.0;
    int i = 0, j = 0;
    int dest = partner_node(*metric_info);

    // Set up domains and contexts, one context per thread
    shmemx_domain_t *domains = (shmemx_domain_t *)malloc(
            nthreads * sizeof(shmemx_domain_t));;
    shmemx_ctx_t *ctxs = (shmemx_ctx_t *)malloc(
            nthreads * sizeof(shmemx_ctx_t));
    assert(domains && ctxs);
    shmemx_domain_create(metric_info->thread_safety, nthreads, domains);

    char **srcs = (char **)malloc(nthreads * sizeof(char *));
    char **dests = (char **)malloc(nthreads * sizeof(char *));
    assert(srcs && dests);

    for (i = 0; i < nthreads; i++) {
        shmemx_ctx_create(domains[i], ctxs + i);
        srcs[i] = aligned_buffer_alloc(len);
        dests[i] = aligned_buffer_alloc(len);
        assert(srcs[i] && dests[i]);
    }

    if (metric_info->window_size % nthreads != 0) {
        fprintf(stderr, "Window size %lu is not evenly partitionable among %d "
                "threads\n", metric_info->window_size, nthreads);
        exit(1);
    }
    const unsigned long int window_size_per_thread = metric_info->window_size /
        nthreads;

    shmem_barrier_all();

    if (streaming_node) {

#pragma omp parallel default(none) firstprivate(window_size_per_thread, ctxs, \
        metric_info, len, dest, srcs, dests) private(j) shared(start)
        {
            int i;
            const int thread_id = omp_get_thread_num();
            const shmemx_ctx_t ctx = ctxs[thread_id];
            char *src_buffer = srcs[thread_id];
            char *dest_buffer = dests[thread_id];

            for (i = 0; i < metric_info->trials + metric_info->warmup; i++) {
                if(thread_id == 0 && i == metric_info->warmup)
                    start = perf_shmemx_wtime();

                for (j = 0; j < window_size_per_thread ; j++) {
                    shmemx_ctx_putmem(dest_buffer, src_buffer, len, dest, ctx);
                }
                shmemx_ctx_quiet(ctx);
#pragma omp barrier // Keep threads in sync
            }
        }
        end = perf_shmemx_wtime();

        calc_and_print_results((end - start), len, *metric_info);
    }

    shmem_barrier_all();

    for (i = 0; i < nthreads; i++) {
        shmemx_ctx_destroy(ctxs[i]);
        shmem_free(srcs[i]);
        shmem_free(dests[i]);
    }
    free(ctxs);
    free(srcs);
    free(dests);
    shmemx_domain_destroy(nthreads, domains);
}
