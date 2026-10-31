#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <getopt.h>
#include <pthread.h>

#include "sum.h"
#include "utils.h"

void *ThreadSum(void *args) {
  struct SumArgs *sum_args = (struct SumArgs *)args;
  sum_args->sum = Sum(sum_args);
  return NULL;
}

int main(int argc, char **argv) {
  uint32_t threads_num = 0;
  uint32_t array_size = 0;
  uint32_t seed = 0;

  static struct option options[] = {
      {"threads_num", required_argument, 0, 1},
      {"seed", required_argument, 0, 2},
      {"array_size", required_argument, 0, 3},
      {0, 0, 0, 0}};

  int c;
  while ((c = getopt_long(argc, argv, "", options, NULL)) != -1) {
    if (c == '?') return 1;

    char *end;
    errno = 0;
    unsigned long value = strtoul(optarg, &end, 10);

    if (errno != 0 || end == optarg || *end != '\0' ||
        optarg[0] == '-' || value == 0 || value > INT_MAX) {
      fprintf(stderr, "Argument must be a positive integer (<= INT_MAX)\n");
      return 1;
    }

    switch (c) {
      case 1: threads_num = (uint32_t)value; break;
      case 2: seed = (uint32_t)value; break;
      case 3: array_size = (uint32_t)value; break;
      default: return 1;
    }
  }

  if (!threads_num || !seed || !array_size || optind != argc ||
      threads_num > array_size) {
    fprintf(stderr,
            "Usage: %s --threads_num num --seed num --array_size num\n"
            "(1 <= threads_num <= array_size)\n", argv[0]);
    return 1;
  }

  pthread_t threads[threads_num];

  int *array = malloc(sizeof(int) * (size_t)array_size);
  if (array == NULL) {
    perror("malloc");
    return 1;
  }

  GenerateArray(array, (int)array_size, (int)seed);

  struct SumArgs args[threads_num];
  struct timespec start_time, finish_time;

  clock_gettime(CLOCK_MONOTONIC, &start_time);

  for (uint32_t i = 0; i < threads_num; i++) {
    args[i].array = array;
    args[i].begin = (int)((uint64_t)i * array_size / threads_num);
    args[i].end = (int)((uint64_t)(i + 1) * array_size / threads_num);
    args[i].sum = 0;

    if (pthread_create(&threads[i], NULL, ThreadSum, (void *)&args[i])) {
      printf("Error: pthread_create failed!\n");
      for (uint32_t j = 0; j < i; j++)
        pthread_join(threads[j], NULL);
      free(array);
      return 1;
    }
  }

  int total_sum = 0;
  for (uint32_t i = 0; i < threads_num; i++) {
    if (pthread_join(threads[i], NULL)) {
      printf("Error: pthread_join failed!\n");
      free(array);
      return 1;
    }
    total_sum += args[i].sum;
  }

  clock_gettime(CLOCK_MONOTONIC, &finish_time);

  double elapsed_time =
      (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time +=
      (finish_time.tv_nsec - start_time.tv_nsec) / 1000000.0;

  free(array);

  printf("Total: %d\n", total_sum);
  printf("Elapsed time: %fms\n", elapsed_time);
  return 0;
}
