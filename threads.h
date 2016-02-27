#ifndef _THREADS_H
#define _THREADS_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#define BUFFER_SIZE 10

typedef struct {
	int cluster_start;
	int cluster_first;
	int cluster_count;
} thread_data;

typedef struct {
	thread_data buffer[BUFFER_SIZE];
	int buffer_index_in;
	int buffer_index_out;
	sem_t filled;
	sem_t empty;
	pthread_mutex_t lock;
	int running;
} producer;

void *defrag_procedure(void *arg);

#endif
