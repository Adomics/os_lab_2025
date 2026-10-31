CC=gcc
CFLAGS=-I.
all: sequential_min_max parallel_min_max par_sequential_min_max process_memory
sequential_min_max : utils.o find_min_max.o utils.h find_min_max.h
	$(CC) -o sequential_min_max find_min_max.o utils.o sequential_min_max.c $(CFLAGS)
parallel_min_max : utils.o find_min_max.o utils.h find_min_max.h
	$(CC) -o parallel_min_max utils.o find_min_max.o parallel_min_max.c $(CFLAGS)
par_sequential_min_max : par_sequential_min_max.c sequential_min_max
	$(CC) -o par_sequential_min_max par_sequential_min_max.c $(CFLAGS)
process_memory: process_memory.c
	$(CC) -std=gnu89 -o process_memory process_memory.c
utils.o : utils.h utils.c
	$(CC) -o utils.o -c utils.c $(CFLAGS)
find_min_max.o : utils.h find_min_max.h find_min_max.c
	$(CC) -o find_min_max.o -c find_min_max.c $(CFLAGS)
clean :
	rm utils.o find_min_max.o sequential_min_max parallel_min_max par_sequential_min_max process_memory
