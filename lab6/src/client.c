#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "common.h"

struct Server {
  char ip[255];
  int port;
};

struct ServerArgs {
  struct Server server;
  uint64_t begin, end, mod, result;
  int ok;
};

bool ConvertStringToUI64(const char *str, uint64_t *val) {
  if (!str || !*str || *str == '-') return false;
  char *end = NULL;
  errno = 0;
  unsigned long long i = strtoull(str, &end, 10);
  if (errno != 0 || *end != '\0') return false;
  *val = (uint64_t)i;
  return true;
}

/* Each connection runs in its own client thread. */
static void *WorkWithServer(void *data) {
  struct ServerArgs *args = data;
  /* getaddrinfo() is thread-safe, unlike gethostbyname() on some systems. */
  struct addrinfo hints = {0}, *addresses = NULL;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  char port[16];
  snprintf(port, sizeof(port), "%d", args->server.port);
  int err = getaddrinfo(args->server.ip, port, &hints, &addresses);
  if (err != 0) {
    fprintf(stderr, "getaddrinfo(%s): %s\n", args->server.ip, gai_strerror(err));
    return NULL;
  }
  int sck = -1;
  for (struct addrinfo *a = addresses; a; a = a->ai_next) {
    sck = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
    if (sck < 0) continue;
    if (connect(sck, a->ai_addr, a->ai_addrlen) == 0) break;
    close(sck);
    sck = -1;
  }
  freeaddrinfo(addresses);
  if (sck < 0) {
    fprintf(stderr, "Connection failed: %s:%d\n", args->server.ip, args->server.port);
    return NULL;
  }

  uint64_t task[3] = {args->begin, args->end, args->mod};
  if (send_all(sck, task, sizeof(task)) != 0 ||
      recv_all(sck, &args->result, sizeof(args->result)) != 0) {
    fprintf(stderr, "Network error with %s:%d\n", args->server.ip, args->server.port);
    close(sck);
    return NULL;
  }
  args->ok = 1;
  close(sck);
  return NULL;
}

int main(int argc, char **argv) {
  uint64_t k = 0;
  bool have_k = false;
  uint64_t mod = 0;
  char servers[255] = {'\0'}; /* Fixed-length buffer for the path to the server list. */

  while (true) {
    static struct option options[] = {{"k", required_argument, 0, 0},
                                      {"mod", required_argument, 0, 0},
                                      {"servers", required_argument, 0, 0},
                                      {0, 0, 0, 0}};
    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);
    if (c == -1) break;
    if (c != 0) { fprintf(stderr, "Arguments error\n"); return 1; }
    switch (option_index) {
      case 0:
        if (!ConvertStringToUI64(optarg, &k)) { fprintf(stderr, "Invalid k\n"); return 1; }
        have_k = true;
        break;
      case 1:
        if (!ConvertStringToUI64(optarg, &mod)) { fprintf(stderr, "Invalid mod\n"); return 1; }
        break;
      case 2:
        if (strlen(optarg) >= sizeof(servers)) { fprintf(stderr, "Servers path too long\n"); return 1; }
        strcpy(servers, optarg);
        break;
    }
  }

  if (!have_k || mod == 0 || !strlen(servers)) {
    fprintf(stderr, "Using: %s --k 1000 --mod 5 --servers /path/to/file\n", argv[0]);
    return 1;
  }

  FILE *file = fopen(servers, "r");
  if (!file) { perror("fopen servers"); return 1; }
  struct Server *to = NULL;
  size_t servers_num = 0;
  char line[512];
  while (fgets(line, sizeof(line), file)) {
    char ip[255], tail;
    int port;
    char *p = line;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p == '#' || *p == '\n' || *p == '\0') continue;
    if (sscanf(p, " %254[^:]:%d %c", ip, &port, &tail) != 2 || port <= 0 || port > 65535) {
      fprintf(stderr, "Invalid server entry: %s", line);
      free(to); fclose(file); return 1;
    }
    if (strlen(ip) == 0) { fprintf(stderr, "Empty IP\n"); free(to); fclose(file); return 1; }
    struct Server *tmp = realloc(to, (servers_num + 1) * sizeof(*to));
    if (!tmp) { perror("realloc"); free(to); fclose(file); return 1; }
    to = tmp;
    snprintf(to[servers_num].ip, sizeof(to[servers_num].ip), "%s", ip);
    to[servers_num].port = port;
    ++servers_num;
  }
  if (ferror(file)) { perror("read servers"); free(to); fclose(file); return 1; }
  fclose(file);
  if (!servers_num) { fprintf(stderr, "Server list is empty\n"); free(to); return 1; }

  /* No network needed for an empty factorial. */
  if (k == 0 || mod == 1) {
    printf("answer: %llu\n", (unsigned long long)(1 % mod));
    free(to);
    return 0;
  }

  /* Don't create connections for empty ranges. */
  if (k < servers_num) servers_num = (size_t)k;
  pthread_t *threads = calloc(servers_num, sizeof(*threads));
  struct ServerArgs *args = calloc(servers_num, sizeof(*args));
  if (!threads || !args) { perror("calloc"); free(threads); free(args); free(to); return 1; }

  size_t created = 0;
  for (size_t i = 0; i < servers_num; ++i) {
    args[i].server = to[i];
    SplitRange(1, k, servers_num, i, &args[i].begin, &args[i].end);
    args[i].mod = mod;
    if (pthread_create(&threads[i], NULL, WorkWithServer, &args[i]) != 0) {
      fprintf(stderr, "pthread_create failed\n");
      break;
    }
    ++created;
  }

  uint64_t answer = 1 % mod;
  int ok = (created == servers_num);
  for (size_t i = 0; i < created; ++i) {
    pthread_join(threads[i], NULL);
    if (!args[i].ok) ok = 0;
    else {
      printf("server %s:%d, range [%llu, %llu], result = %llu\n",
             args[i].server.ip, args[i].server.port,
             (unsigned long long)args[i].begin, (unsigned long long)args[i].end,
             (unsigned long long)args[i].result);
      answer = MultModulo(answer, args[i].result, mod);
    }
  }
  if (ok) printf("answer: %llu\n", (unsigned long long)answer);
  free(threads); free(args); free(to);
  return ok ? 0 : 1;
}
