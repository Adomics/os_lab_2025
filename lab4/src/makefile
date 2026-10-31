CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -pthread
UTILS_DIR ?= ../../lab3/src
all: parallel_sum
parallel_sum: parallel_sum.o libsum.a utils.o
	$(CC) $(CFLAGS) -o $@ parallel_sum.o libsum.a utils.o
parallel_sum.o: parallel_sum.c sum.h $(UTILS_DIR)/utils.h
	$(CC) $(CFLAGS) -I$(UTILS_DIR) -c $< -o $@
sum.o: sum.c sum.h
	$(CC) $(CFLAGS) -c $< -o $@
libsum.a: sum.o
	ar rcs $@ $<
utils.o: $(UTILS_DIR)/utils.c $(UTILS_DIR)/utils.h
	$(CC) $(CFLAGS) -I$(UTILS_DIR) -c $< -o $@
clean:
	rm -f parallel_sum parallel_sum.o sum.o libsum.a utils.o
