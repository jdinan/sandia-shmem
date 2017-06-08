/*
*
*  Copyright (c) 2015 Intel Corporation. All rights reserved.
*  This software is available to you under the BSD license. For
*  license information, see the LICENSE file in the top level directory.
*
*/

#include <common.h>
#include <shmemx.h>

#define PUT_IO_NODE 1
#define GET_IO_NODE !PUT_IO_NODE
#define INIT_VALUE 1

#define MAX_MSG_SIZE (1<<23)
#define START_LEN 1

#define INC 2
#define TRIALS 100
#define WARMUP 10

typedef struct perf_metrics {
   unsigned int start_len, max_len;
   unsigned int inc, trials;
   unsigned int warmup;
   int validate;
   int my_node, npes;
   long * target;
   char * src, *dest;
   int thread_safety;
   int domain_thread_safety;
   int nthreads;
   shmemx_domain_t *domains;
   shmemx_ctx_t *ctxs;
} perf_metrics_t;

void static data_init(perf_metrics_t * data) {
   data->start_len = START_LEN;
   data->max_len = MAX_MSG_SIZE;
   data->inc = INC;
   data->trials = TRIALS;
   data->warmup = WARMUP; /*number of initial iterations to skip*/
   data->validate = false;
   data->my_node = shmem_my_pe();
   data->npes = shmem_n_pes();
   data->target = NULL;
   data->src = NULL;
   data->dest = NULL;
   data->thread_safety = SHMEMX_THREAD_SINGLE;
   data->domain_thread_safety = SHMEMX_THREAD_SINGLE;
   data->nthreads = 1;
   data->domains = NULL;
   data->ctxs = NULL;
}

void static inline print_results_header(void) {
   printf("\nLength                  Latency                       \n");
   printf("in bytes            in micro seconds              \n");
}

/*not storing results, only outputing it*/
void static inline calc_and_print_results(double elapsed_cpu_time, int len,
                                         perf_metrics_t data, const int use_contexts) {
    double latency = 0.0;
    if (use_contexts) {
        latency = elapsed_cpu_time / (data.trials * data.nthreads);
    } else {
        latency = elapsed_cpu_time / data.trials;
    }

    printf("%9d           %8.2f             \n", len, latency);
}

int static inline partner_node(int my_node)
{
    return ((my_node % 2 == 0) ? (my_node + 1) : (my_node - 1));
}

void static inline command_line_arg_check(int argc, char *argv[],
                            perf_metrics_t *metric_info) {
    int ch, error = false;
    extern char *optarg;

    /* check command line args */
    while ((ch = getopt(argc, argv, "e:s:n:vc:t:d:")) != EOF) {
        switch (ch) {
        case 's':
            metric_info->start_len = strtol(optarg, (char **)NULL, 0);
            if ( metric_info->start_len < 1 ) metric_info->start_len = 1;
            if(!is_pow_of_2(metric_info->start_len)) error = true;
            break;
        case 'e':
            metric_info->max_len = strtol(optarg, (char **)NULL, 0);
            if(!is_pow_of_2(metric_info->max_len)) error = true;
            if(metric_info->max_len < metric_info->start_len) error = true;
            break;
        case 'n':
            metric_info->trials = strtol(optarg, (char **)NULL, 0);
            if(metric_info->trials <= (metric_info->warmup*2)) error = true;
            break;
        case 'v':
            metric_info->validate = true;
            break;
        case 'c':
            if (strcmp(optarg, "SINGLE") == 0) {
                metric_info->thread_safety = SHMEMX_THREAD_SINGLE;
            } else if (strcmp(optarg, "FUNNELED") == 0) {
                metric_info->thread_safety = SHMEMX_THREAD_FUNNELED;
            } else if (strcmp(optarg, "SERIALIZED") == 0) {
                metric_info->thread_safety = SHMEMX_THREAD_SERIALIZED;
            } else if (strcmp(optarg, "MULTIPLE") == 0) {
                metric_info->thread_safety = SHMEMX_THREAD_MULTIPLE;
            } else {
                fprintf(stderr, "Unexpected value for -c: \"%s\"\n", optarg);
                error = true;
            }
            break;
        case 'd':
            if (strcmp(optarg, "SINGLE") == 0) {
                metric_info->domain_thread_safety = SHMEMX_THREAD_SINGLE;
            } else if (strcmp(optarg, "FUNNELED") == 0) {
                metric_info->domain_thread_safety = SHMEMX_THREAD_FUNNELED;
            } else if (strcmp(optarg, "SERIALIZED") == 0) {
                metric_info->domain_thread_safety = SHMEMX_THREAD_SERIALIZED;
            } else if (strcmp(optarg, "MULTIPLE") == 0) {
                metric_info->domain_thread_safety = SHMEMX_THREAD_MULTIPLE;
            } else {
                fprintf(stderr, "Unexpected value for -d: \"%s\"\n", optarg);
                error = true;
            }
            break;
        case 't':
            metric_info->nthreads = atoi(optarg);
            break;
        default:
            error = true;
            break;
        }
    }

    if (error) {
        if (metric_info->my_node == 0) {
            fprintf(stderr, "Usage: [-s start_length] [-e end_length] "\
                    ": lengths must be a power of two \n " \
                    "[-n trials (must be greater than 20)] "\
                    "[-c runtime-thread-safety-config] \n"\
                    "[-d domain-thread-safety-config] \n"\
                    "[-t num-threads] \n"\
                    "[-v (validate results)]\n");
        }
#ifndef VERSION_1_0
        shmem_finalize();
#endif
        exit (-1);
    }
}

void static inline only_two_PEs_check(int my_node, int num_pes) {
    if (num_pes != 2) {
        if (my_node == 0) {
            fprintf(stderr, "2-nodes only test\n");
        }
#ifndef VERSION_1_0
        shmem_finalize();
#endif
        exit(77);
    }
}

/**************************************************************/
/*                   Latency data gathering                   */
/**************************************************************/

/*have single symmetric long element "target" from perf_metrics_t
 *  that needs to be initialized in function*/
extern void long_element_round_trip_latency(perf_metrics_t data);

extern void int_element_latency(perf_metrics_t data);

/*have symmetric buffers src/dest from perf_metrics_t
 *  that has been initialized to my_node number */
extern void streaming_latency(int len, perf_metrics_t *data);

void static inline  multi_size_latency(perf_metrics_t data, char *argv[]) {
    int len;
    int partner_pe = partner_node(data.my_node);

    for (len = data.start_len; len <= data.max_len; len *= data.inc) {

        shmem_barrier_all();

        streaming_latency(len, &data);

        shmem_barrier_all();
    }

    shmem_barrier_all();

    if((data.my_node == 0) && data.validate)
        validate_recv(data.dest, data.max_len, partner_pe);
}



/**************************************************************/
/*                   INIT and teardown of resources           */
/**************************************************************/

void static inline latency_init_resources(int argc, char *argv[],
                                          perf_metrics_t *data,
                                          const int use_contexts) {
#ifndef VERSION_1_0
    int tl;
    shmemx_init_thread(data->thread_safety, &tl);
    if (tl != data->thread_safety) {
        fprintf(stderr, "Could not initialize with requested thread level %d: "
                "got %d\n", data->thread_safety, tl);
        exit(-1);
    }
#else
    start_pes(0);
#endif

    data_init(data);

    only_two_PEs_check(data->my_node, data->npes);

    command_line_arg_check(argc, argv, data);

#ifndef VERSION_1_0
    if (use_contexts) {
        int i;
        // Set up domains and contexts, one context per thread
        data->domains = (shmemx_ctx_t *)malloc(
                data->nthreads * sizeof(shmemx_domain_t));
        data->ctxs = (shmemx_ctx_t *)malloc(
                data->nthreads * sizeof(shmemx_ctx_t));
        assert(data->domains && data->ctxs);
        shmemx_domain_create(data->domain_thread_safety,
                data->nthreads, data->domains);
        for (i = 0; i < data->nthreads; i++) {
            shmemx_ctx_create(data->domains[i], data->ctxs + i);
        }
    }
#endif

    data->src = aligned_buffer_alloc(data->max_len);
    init_array(data->src, data->max_len, data->my_node);

    data->dest = aligned_buffer_alloc(data->max_len);
    init_array(data->dest, data->max_len, data->my_node);

#ifndef VERSION_1_0
    data->target = shmem_malloc(sizeof(long));
#else
    data->target = shmalloc(sizeof(long));
#endif
}

void static inline latency_free_resources(perf_metrics_t *data,
                                          const int use_contexts) {
    shmem_barrier_all();

#ifndef VERSION_1_0
    shmem_free(data->target);
#else
    shfree(data->target);
#endif
    aligned_buffer_free(data->src);
    aligned_buffer_free(data->dest);
#ifndef VERSION_1_0
    if (use_contexts) {
        int i;
        for (i = 0; i < data->nthreads; i++) {
            shmemx_ctx_destroy(data->ctxs[i]);
        }
        shmemx_domain_destroy(data->nthreads, data->domains);
        free(data->ctxs);
        free(data->domains);
    }
    shmem_finalize();
#endif
}

void static inline latency_main(int argc, char *argv[],
        const int use_contexts) {
    perf_metrics_t data;

    latency_init_resources(argc, argv, &data, use_contexts);

    long_element_round_trip_latency(data);

    int_element_latency(data);

    multi_size_latency(data, argv);

    latency_free_resources(&data, use_contexts);
}
