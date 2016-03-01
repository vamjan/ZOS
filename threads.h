#ifndef _THREADS_H
#define _THREADS_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#define BUFFER_SIZE 10 	//velikost spoleèného bufferu producenta
#define TASK_SIZE 25	//velikost 1 úlohy v clusterech, producent vytváøí úlohy o této velikosti

//struktura sloužící k pøedávání dat vláknùm
typedef struct {
	int cluster_start;	//místo kam má zaèít zapisovat
	int cluster_first;	//první cluster zadané úlohy
	int cluster_count;	//poèet clusterù úlohy
	int cluster_file;	//index souboru, ve kterém se úloha nachází
} thread_data;

//struktura producenta
typedef struct {
	thread_data buffer[BUFFER_SIZE];	//buffer úloh
	int buffer_index_in;				//poèet vytvoøených úloh
	int buffer_index_out;				//poèet vybraných úloh
	sem_t filled;						//semafor urèující poèet úloh èekajících na pøedání
	sem_t empty;						//semafor urèující zbylé místo v bufferu
	pthread_mutex_t lock;				//mutex pro pøístup k bufferu
	int running;						//znaèení bìhu producenta
} producer;

//funkce pro paralelní výpoèet
void free_locks();
pthread_t **create_threads(int count, void *(*function)(void *));
int join_threads(int count, pthread_t **threads);

pthread_mutex_t **create_cluster_locks(int count);
void setup_locks();

thread_data *get_job();
void add_job(thread_data td);
void create_jobs(int count, int function);
void setup_producer();

void swap(int point1, int point2, int cluster1, int cluster2);
void defrag(int file, int start, int count, int first_cluster);
void *defrag_procedure(void *arg);

void control(int file, int start, int count);
void *control_procedure(void *arg);

#endif
