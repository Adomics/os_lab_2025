#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t result = 1;
static pthread_mutex_t result_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    uint64_t begin;
    uint64_t end;
    uint64_t mod;
} ThreadArgs;

static uint64_t mul_mod(uint64_t a, uint64_t b, uint64_t mod) {
    return (uint64_t)(((__uint128_t)a * b) % mod);
}

static void *calculate_part(void *data) {
    ThreadArgs *args = (ThreadArgs *)data;
    uint64_t part = 1 % args->mod;

    for (uint64_t i = args->begin;; ++i) {
        part = mul_mod(part, i % args->mod, args->mod);
        if (i == args->end)
            break;
    }

    pthread_mutex_lock(&result_mutex);
    result = mul_mod(result, part, args->mod);
    pthread_mutex_unlock(&result_mutex);
    return NULL;
}

static int parse_uint64(const char *text, uint64_t *out) {
    if (!text || !*text || *text == '-')
        return -1;

    errno = 0;
    char *end;
    unsigned long long value = strtoull(text, &end, 10);
    if (errno == ERANGE || *end != '\0')
        return -1;

    *out = (uint64_t)value;
    return 0;
}

int main(int argc, char **argv) {
    uint64_t k = 0, pnum = 0, mod = 0;
    int has_k = 0, has_pnum = 0, has_mod = 0;
    const struct option options[] = {
        {"k", required_argument, NULL, 'k'},
        {"pnum", required_argument, NULL, 'p'},
        {"mod", required_argument, NULL, 'm'},
        {NULL, 0, NULL, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "k:p:m:", options, NULL)) != -1) {
        switch (opt) {
            case 'k':
                if (parse_uint64(optarg, &k) != 0) goto invalid;
                has_k = 1;
                break;
            case 'p':
                if (parse_uint64(optarg, &pnum) != 0) goto invalid;
                has_pnum = 1;
                break;
            case 'm':
                if (parse_uint64(optarg, &mod) != 0) goto invalid;
                has_mod = 1;
                break;
            default:
                goto invalid;
        }
    }

    if (!has_k || !has_pnum || !has_mod || optind != argc || pnum == 0 || mod == 0)
        goto invalid;

    if (k == 0) {
        printf("0! mod %" PRIu64 " = %" PRIu64 "\n", mod, (uint64_t)(1 % mod));
        return 0;
    }

    uint64_t thread_count_u64 = (pnum < k) ? pnum : k;
    if (thread_count_u64 > SIZE_MAX / sizeof(pthread_t) ||
        thread_count_u64 > SIZE_MAX / sizeof(ThreadArgs)) {
        fprintf(stderr, "Слишком много потоков.\n");
        return 1;
    }
    size_t thread_count = (size_t)thread_count_u64;
    pthread_t *threads = calloc(thread_count, sizeof(*threads));
    ThreadArgs *args = calloc(thread_count, sizeof(*args));
    if (!threads || !args) {
        fprintf(stderr, "Не удалось выделить память.\n");
        free(threads);
        free(args);
        return 1;
    }

    result = 1 % mod;
    uint64_t base = k / thread_count_u64;
    uint64_t extra = k % thread_count_u64;
    size_t created = 0;

    for (size_t i = 0; i < thread_count; ++i) {
        uint64_t idx = (uint64_t)i;
        uint64_t len = base + (idx < extra ? 1 : 0);
        uint64_t begin = 1 + idx * base + (idx < extra ? idx : extra);
        args[i] = (ThreadArgs){begin, begin + len - 1, mod};

        int err = pthread_create(&threads[i], NULL, calculate_part, &args[i]);
        if (err != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(err));
            break;
        }
        created++;
    }

    int failed = created != thread_count;
    for (size_t i = 0; i < created; ++i) {
        int err = pthread_join(threads[i], NULL);
        if (err != 0) {
            fprintf(stderr, "pthread_join: %s\n", strerror(err));
            failed = 1;
        }
    }

    if (!failed)
        printf("%" PRIu64 "! mod %" PRIu64 " = %" PRIu64 "\n", k, mod, result);

    pthread_mutex_destroy(&result_mutex);
    free(threads);
    free(args);
    return failed ? 1 : 0;

invalid:
    fprintf(stderr, "Использование: %s -k K --pnum=P --mod=M\n", argv[0]);
    fprintf(stderr, "Условия: K >= 0, P >= 1, M >= 1.\n");
    return 1;
}
