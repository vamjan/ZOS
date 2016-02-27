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
	int cluster_file;
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

void free_locks();
pthread_t **create_threads(int count, void *(*function)(void *));
int join_threads(int count, pthread_t **threads);
pthread_mutex_t **create_cluster_locks(int count);
void setup_locks(int threads);
thread_data *get_job();
void add_job(thread_data td);
void create_jobs(int lenght, int count, int function);
void setup_producer();
void swap(int point1, int point2, int cluster1, int cluster2);
void defrag(int file, int start, int count, int first_cluster);
void *defrag_procedure(void *arg);
void control(int file, int start, int count);
void *control_procedure(void *arg);

#endif
