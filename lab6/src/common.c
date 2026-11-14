#include "common.h"

#include <errno.h>
#include <sys/socket.h>

uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod) {
  return (uint64_t)(((__uint128_t)a * b) % mod);
}

int send_all(int fd, const void *buf, size_t len) {
  const char *p = buf;
  while (len) {
    ssize_t n = send(fd, p, len, 0);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) return -1;
    p += n;
    len -= (size_t)n;
  }
  return 0;
}

int recv_all(int fd, void *buf, size_t len) {
  char *p = buf;
  while (len) {
    ssize_t n = recv(fd, p, len, 0);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) return -1;
    p += n;
    len -= (size_t)n;
  }
  return 0;
}

void SplitRange(uint64_t begin, uint64_t count, size_t parts, size_t index,
                uint64_t *out_begin, uint64_t *out_end) {
  uint64_t n = (uint64_t)parts;
  uint64_t i = (uint64_t)index;
  uint64_t base = count / n;
  uint64_t rem = count % n;
  uint64_t extra_before = i < rem ? i : rem;
  uint64_t chunk = base + (i < rem);
  *out_begin = begin + i * base + extra_before;
  *out_end = *out_begin + chunk - 1;
}
