#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdint.h>

/* Modular multiplication without overflowing uint64_t (GCC/Clang). mod > 0. */
uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod);

/* Transfer exactly len bytes over a TCP socket; return 0 or -1 on error/EOF. */
int send_all(int fd, const void *buf, size_t len);
int recv_all(int fd, void *buf, size_t len);

/* Divide [begin, begin + count - 1] into parts nonempty chunks.
 * Preconditions: parts > 0, count >= parts, index < parts.
 * Extra elements are assigned to chunks with smaller indices. */
void SplitRange(uint64_t begin, uint64_t count, size_t parts, size_t index,
                uint64_t *out_begin, uint64_t *out_end);

#endif
