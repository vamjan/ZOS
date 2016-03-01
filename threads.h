#ifndef _THREADS_H
#define _THREADS_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#define BUFFER_SIZE 10 	//velikost spole�n�ho bufferu producenta
#define TASK_SIZE 25	//velikost 1 �lohy v clusterech, producent vytv��� �lohy o t�to velikosti

//struktura slou��c� k p�ed�v�n� dat vl�kn�m
typedef struct {
	int cluster_start;	//m�sto kam m� za��t zapisovat
	int cluster_first;	//prvn� cluster zadan� �lohy
	int cluster_count;	//po�et cluster� �lohy
	int cluster_file;	//index souboru, ve kter�m se �loha nach�z�
} thread_data;

//struktura producenta
typedef struct {
	thread_data buffer[BUFFER_SIZE];	//buffer �loh
	int buffer_index_in;				//po�et vytvo�en�ch �loh
	int buffer_index_out;				//po�et vybran�ch �loh
	sem_t filled;						//semafor ur�uj�c� po�et �loh �ekaj�c�ch na p�ed�n�
	sem_t empty;						//semafor ur�uj�c� zbyl� m�sto v bufferu
	pthread_mutex_t lock;				//mutex pro p��stup k bufferu
	int running;						//zna�en� b�hu producenta
} producer;

//funkce pro paraleln� v�po�et
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
