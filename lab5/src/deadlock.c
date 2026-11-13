#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t barrier;

void *thread_one(void *arg) {
    (void)arg;
    pthread_mutex_lock(&mutex1);
    puts("Thread 1: locked mutex1");

    pthread_barrier_wait(&barrier);

    puts("Thread 1: waiting for mutex2...");
    pthread_mutex_lock(&mutex2);  // Never succeeds: thread 2 owns mutex2.

    pthread_mutex_unlock(&mutex2);
    pthread_mutex_unlock(&mutex1);
    return NULL;
}

void *thread_two(void *arg) {
    (void)arg;
    pthread_mutex_lock(&mutex2);
    puts("Thread 2: locked mutex2");

    pthread_barrier_wait(&barrier);

    puts("Thread 2: waiting for mutex1...");
    pthread_mutex_lock(&mutex1);  // Never succeeds: thread 1 owns mutex1.

    pthread_mutex_unlock(&mutex1);
    pthread_mutex_unlock(&mutex2);
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    setvbuf(stdout, NULL, _IONBF, 0);

    if (pthread_barrier_init(&barrier, NULL, 2) != 0) {
        fputs("Cannot initialize barrier\n", stderr);
        return EXIT_FAILURE;
    }
    if (pthread_create(&t1, NULL, thread_one, NULL) != 0 ||
        pthread_create(&t2, NULL, thread_two, NULL) != 0) {
        fputs("Cannot create threads\n", stderr);
        return EXIT_FAILURE;
    }

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    // Unreachable in the deadlock demonstration.
    pthread_barrier_destroy(&barrier);
    pthread_mutex_destroy(&mutex1);
    pthread_mutex_destroy(&mutex2);
    return 0;
}
