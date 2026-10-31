#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

// Проверяем, истёк ли таймаут, используя монотонные часы.
static bool TimeoutExceeded(const struct timespec *start, int timeout) {
  struct timespec now;
  if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
    perror("clock_gettime");
    exit(1);
  }
  time_t seconds = now.tv_sec - start->tv_sec;
  return seconds > timeout ||
         (seconds == timeout && now.tv_nsec >= start->tv_nsec);
}

int main(int argc, char **argv) {
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  bool with_files = false;
  int timeout = -1;  // По умолчанию без ограничения времени.

  while (true) {
    static struct option options[] = {{"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {"pnum", required_argument, 0, 0},
                                      {"by_files", no_argument, 0, 'f'},
                                      {"timeout", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "f", options, &option_index);

    if (c == -1) break;

    switch (c) {
      case 0:
        switch (option_index) {
          case 0:
            seed = atoi(optarg);
            if (seed <= 0) {
              printf("seed is a positive number\n");
              return 1;
            }
            break;
          case 1:
            array_size = atoi(optarg);
            if (array_size <= 0) {
              printf("array_size is a positive number\n");
              return 1;
            }
            break;
          case 2:
            pnum = atoi(optarg);
            if (pnum <= 0) {
              printf("pnum is a positive number\n");
              return 1;
            }
            break;
          case 3:
            with_files = true;
            break;
          case 4: {
            char *end;
            errno = 0;
            long seconds = strtol(optarg, &end, 10);
            if (errno != 0 || end == optarg || *end != '\0' ||
                seconds <= 0 || seconds > INT_MAX) {
              printf("timeout must be a positive integer (seconds)\n");
              return 1;
            }
            timeout = (int)seconds;
            break;
          }

          default:
            printf("Index %d is out of options\n", option_index);
        }
        break;
      case 'f':
        with_files = true;
        break;

      case '?':
        break;

      default:
        printf("getopt returned character code 0%o?\n", c);
    }
  }

  if (optind < argc) {
    printf("Has at least one no option argument\n");
    return 1;
  }

  if (seed == -1 || array_size == -1 || pnum == -1) {
    printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" [--timeout seconds] [--by_files]\n",
           argv[0]);
    return 1;
  }

  if (pnum > array_size) {
    printf("pnum must not exceed array_size\n");
    return 1;
  }

  int *array = malloc(sizeof(int) * array_size);
  if (array == NULL) {
    perror("malloc");
    return 1;
  }
  GenerateArray(array, array_size, seed);
  int active_child_processes = 0;

  int pipes[pnum][2];
  FILE *files[pnum];
  pid_t child_pids[pnum];
  int spawned = 0;
  bool timed_out = false;

  struct timeval start_time;
  gettimeofday(&start_time, NULL);
  struct timespec timeout_start;
  if (timeout > 0 && clock_gettime(CLOCK_MONOTONIC, &timeout_start) == -1) {
    perror("clock_gettime");
    free(array);
    return 1;
  }

  for (int i = 0; i < pnum; i++) {
    if (timeout > 0 && TimeoutExceeded(&timeout_start, timeout)) {
      timed_out = true;
      break;
    }
    if (with_files) {
      files[i] = tmpfile();
      if (files[i] == NULL) {
        perror("tmpfile");
        return 1;
      }
    } else {
      if (pipe(pipes[i]) == -1) {
        perror("pipe");
        return 1;
      }
    }

    pid_t child_pid = fork();
    if (child_pid >= 0) {
      active_child_processes += 1;
      if (child_pid == 0) {
        int begin = (int)((long long)i * array_size / pnum);
        int end = (int)((long long)(i + 1) * array_size / pnum);
        struct MinMax local = GetMinMax(array, begin, end);

        if (with_files) {
          if (fwrite(&local, sizeof(local), 1, files[i]) != 1) {
            perror("fwrite");
            return 1;
          }
          fclose(files[i]);
        } else {
          for (int j = 0; j < i; j++) close(pipes[j][0]);
          close(pipes[i][0]);
          if (write(pipes[i][1], &local, sizeof(local)) != (ssize_t)sizeof(local)) {
            perror("write");
            return 1;
          }
          close(pipes[i][1]);
        }
        return 0;
      }
      child_pids[spawned++] = child_pid;
      if (!with_files) close(pipes[i][1]);

    } else {
      printf("Fork failed!\n");
      return 1;
    }
  }

  while (active_child_processes > 0 && !timed_out) {
    int status;
    pid_t finished = waitpid(-1, &status, timeout > 0 ? WNOHANG : 0);
    if (finished == 0) {  // Дети ещё работают: проверяем таймаут.
      if (TimeoutExceeded(&timeout_start, timeout)) {
        timed_out = true;
        break;
      }
      struct timespec pause_time = {0, 10000000};  // 10 мс.
      nanosleep(&pause_time, NULL);
      continue;
    }
    if (finished == -1) {
      if (errno == EINTR) continue;
      perror("waitpid");
      return 1;
    }
    for (int i = 0; i < spawned; i++) {
      if (child_pids[i] == finished) child_pids[i] = 0;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      printf("Child process failed!\n");
      return 1;
    }
    active_child_processes -= 1;
  }

  if (timed_out) {
    // SIGKILL только дочерним процессам, завершение которых ещё не обработано.
    for (int i = 0; i < spawned; i++) {
      if (child_pids[i] > 0) kill(child_pids[i], SIGKILL);
    }
    // Ожидаем завершения убитых процессов, чтобы не оставлять зомби.
    while (active_child_processes > 0) {
      if (wait(NULL) > 0) active_child_processes--;
      else if (errno != EINTR) break;
    }
    if (with_files) {
      for (int i = 0; i < spawned; i++) fclose(files[i]);
    } else {
      for (int i = 0; i < spawned; i++) close(pipes[i][0]);
    }
    free(array);
    fprintf(stderr, "Timeout exceeded (%d s): child processes killed.\n", timeout);
    return 124;
  }

  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;

  for (int i = 0; i < pnum; i++) {
    int min = INT_MAX;
    int max = INT_MIN;
    struct MinMax local;

    if (with_files) {
      rewind(files[i]);
      if (fread(&local, sizeof(local), 1, files[i]) != 1) {
        printf("Cannot read file %d\n", i);
        return 1;
      }
      fclose(files[i]);
    } else {
      if (read(pipes[i][0], &local, sizeof(local)) != (ssize_t)sizeof(local)) {
        printf("Cannot read pipe %d\n", i);
        return 1;
      }
      close(pipes[i][0]);
    }

    min = local.min;
    max = local.max;

    if (min < min_max.min) min_max.min = min;
    if (max > min_max.max) min_max.max = max;
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(array);

  printf("Min: %d\n", min_max.min);
  printf("Max: %d\n", min_max.max);
  printf("Elapsed time: %fms\n", elapsed_time);
  fflush(NULL);
  return 0;
}
