#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <getopt.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <pthread.h>

#include "common.h"

struct FactorialArgs {
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
  uint64_t result;
};

uint64_t Factorial(const struct FactorialArgs *args) {
  uint64_t ans = 1 % args->mod;
  for (uint64_t i = args->begin; i <= args->end; ) {
    ans = MultModulo(ans, i, args->mod);
    if (i == args->end) break; /* Avoid uint64_t wraparound. */
    ++i;
  }
  return ans;
}

void *ThreadFactorial(void *data) {
  struct FactorialArgs *fargs = (struct FactorialArgs *)data;
  fargs->result = Factorial(fargs);
  return NULL;
}

int main(int argc, char **argv) {
  int tnum = -1;
  int port = -1;

  while (true) {
    static struct option options[] = {{"port", required_argument, 0, 0},
                                      {"tnum", required_argument, 0, 0},
                                      {0, 0, 0, 0}};
    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);
    if (c == -1) break;
    if (c != 0) { fprintf(stderr, "Unknown argument\n"); return 1; }
    switch (option_index) {
      case 0: port = atoi(optarg); break;
      case 1: tnum = atoi(optarg); break;
    }
  }

  if (port <= 0 || port > 65535 || tnum <= 0) {
    fprintf(stderr, "Using: %s --port 20001 --tnum 4\n", argv[0]);
    return 1;
  }

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) { perror("socket"); return 1; }
  struct sockaddr_in server = {0};
  server.sin_family = AF_INET;
  server.sin_port = htons((uint16_t)port);
  server.sin_addr.s_addr = htonl(INADDR_ANY);

  int opt_val = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));
  if (bind(server_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
    perror("bind"); close(server_fd); return 1;
  }
  if (listen(server_fd, 128) < 0) {
    perror("listen"); close(server_fd); return 1;
  }
  printf("Server listening at %d\n", port);
  fflush(stdout);

  while (true) {
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    int client_fd = accept(server_fd, (struct sockaddr *)&client, &client_len);
    if (client_fd < 0) { perror("accept"); continue; }

    uint64_t task[3];
    while (recv_all(client_fd, task, sizeof(task)) == 0) {
      uint64_t begin = task[0], end = task[1], mod = task[2];
      printf("Receive: %llu %llu %llu\n", (unsigned long long)begin,
             (unsigned long long)end, (unsigned long long)mod);
      fflush(stdout);
      if (mod == 0 || begin == 0 || end < begin) {
        fprintf(stderr, "Invalid task\n");
        break;
      }

      uint64_t count = end - begin + 1;
      size_t n = (uint64_t)tnum < count ? (size_t)tnum : (size_t)count;
      pthread_t *threads = calloc(n, sizeof(*threads));
      struct FactorialArgs *args = calloc(n, sizeof(*args));
      if (!threads || !args) {
        perror("calloc"); free(threads); free(args); break;
      }

      size_t created = 0;
      for (size_t i = 0; i < n; ++i) {
        SplitRange(begin, count, n, i, &args[i].begin, &args[i].end);
        args[i].mod = mod;
        if (pthread_create(&threads[i], NULL, ThreadFactorial, &args[i]) != 0) {
          fprintf(stderr, "pthread_create failed\n");
          break;
        }
        ++created;
      }

      uint64_t total = 1 % mod;
      for (size_t i = 0; i < created; ++i) {
        pthread_join(threads[i], NULL);
        total = MultModulo(total, args[i].result, mod);
      }
      free(threads); free(args);
      if (created != n) break;
      printf("Total: %llu\n", (unsigned long long)total);
      fflush(stdout);
      if (send_all(client_fd, &total, sizeof(total)) != 0) {
        fprintf(stderr, "Can't send data to client\n");
        break;
      }
    }
    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
  }
  close(server_fd);
  return 0;
}
