#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include "fat.h"
#include "threads.h"

pthread_mutex_t **cluster_locks;
pthread_mutex_t *fat_lock;
producer *factory;
int task_size;

void free_locks() {
	int i;
	
	for(i = 0; i < p_boot_record->cluster_count; i++) {
		free(cluster_locks[i]);
	}
	
	free(cluster_locks);
	free(fat_lock);
	free(factory);
}

pthread_t **create_threads(int count, void *(*function)(void *)) {
	int i;
	pthread_t **retval = malloc(sizeof(pthread_t *) * count);
	for(i = 0; i < count; i++) {
		retval[i] = malloc(sizeof(pthread_t));
		pthread_create(retval[i], NULL, function, NULL);
		printf("Thread %d status: STARTUJI\n", i);
	}
	
	return retval;
}

int join_threads(int count, pthread_t **threads) {
	int i;
	for(i = 0; i < count; i++) {
		pthread_join(*threads[i], NULL);
		printf("Thread %d status: KONCIM\n", i);
	}
}

pthread_mutex_t **create_cluster_locks(int count) {
	int i;
	pthread_mutex_t **retval = malloc(sizeof(pthread_mutex_t *) * count);
	for(i = 0; i < count; i++) {
		retval[i] = malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(retval[i], NULL);
	}
	
	return retval;
}

void setup_locks(int threads) {
	cluster_locks = create_cluster_locks(p_boot_record->cluster_count);
	fat_lock = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(fat_lock, NULL);
}

thread_data *get_job() {
	thread_data *retval = malloc(sizeof(thread_data));
	sem_wait(&factory->filled);
	pthread_mutex_lock(&factory->lock);
	
	if(factory->buffer_index_out != factory->buffer_index_in ) {
		memcpy(retval, &factory->buffer[factory->buffer_index_out%BUFFER_SIZE], sizeof(thread_data));
		factory->buffer_index_out++;
	} else {
		retval = NULL;
	}
	
	pthread_mutex_unlock(&factory->lock);
	sem_post(&factory->empty);
	return retval;
}

void add_job(thread_data td) {
	sem_wait(&factory->empty);
	pthread_mutex_lock(&factory->lock);
	
	factory->buffer[factory->buffer_index_in%BUFFER_SIZE] = td;
    factory->buffer_index_in++;
    
    pthread_mutex_unlock(&factory->lock);
    sem_post(&factory->filled);
}

void create_jobs(int lenght, int count, int function) {
	int i, curr;
	thread_data td;
	task_size = lenght;
	printf("Producer startuje. . .\n");
    for(i = 0; i < p_boot_record->root_directory_max_entries_count; i++) {
    	curr = p_root_directory[i]->first_cluster;
    	int job_count = 0, cluster_count = 0;
		td.cluster_first = curr;
    	
    	if(function) td.cluster_start = 0;
    	td.cluster_file = i;
    	
    	while(curr != FAT_FILE_END && curr != FAT_UNUSED && curr != FAT_BAD_CLUSTER) {
			curr = new_fat[curr];
			if(++cluster_count == lenght) {
				td.cluster_count = cluster_count;
    			
    			add_job(td);
    			
    			job_count++;
    			
    			td.cluster_first = curr;
    			
    			if(function) td.cluster_start = cluster_count*job_count;
    			td.cluster_file = i;
    			
				cluster_count = 0;
    		}
    	}
    	td.cluster_count = cluster_count;
    	cluster_count = 0;
    	
    	if(td.cluster_count != 0) {
	    	add_job(td);
	    }
    }
    
    factory->running = 0;
    
    for(i = 0; i < count; i++) {
    	sem_post(&factory->filled);
    }
    printf("Producer hotovo. . .\n");
}

void setup_producer() {
	factory = malloc(sizeof(producer));
	factory->running = 1;
	factory->buffer_index_in = 0;
	factory->buffer_index_out = 0;
	sem_init(&factory->filled, 0, 0);
    sem_init(&factory->empty, 0, BUFFER_SIZE);
    pthread_mutex_init(&factory->lock, NULL);
}

void swap(int point1, int point2, int cluster1, int cluster2) {
	int i;
	if(cluster1 >= p_boot_record->cluster_count || cluster2 >= p_boot_record->cluster_count) return;
    if(cluster1 == cluster2) { return; }
    
    pthread_mutex_lock(fat_lock);
    swap_fat(point1, point2, cluster1, cluster2);
    pthread_mutex_unlock(fat_lock);
    
    pthread_mutex_lock(cluster_locks[cluster1]);
    pthread_mutex_lock(cluster_locks[cluster2]);
    swap_clusters(cluster1, cluster2);
    pthread_mutex_unlock(cluster_locks[cluster2]);
    pthread_mutex_unlock(cluster_locks[cluster1]);
    
    pthread_mutex_lock(fat_lock);
    for(i = 0; i < p_boot_record->root_directory_max_entries_count; i++) {
		if(cluster1 == p_root_directory[i]->first_cluster)
			p_root_directory[i]->first_cluster = cluster2;
		else if(cluster2 == p_root_directory[i]->first_cluster)
			p_root_directory[i]->first_cluster = cluster1;
	}
	pthread_mutex_unlock(fat_lock);
}

void defrag(int file, int start, int count, int first_cluster) {
	int i, curr = first_cluster, start_cluster = correct_first_cluster(file, start);
	
	pthread_mutex_lock(fat_lock);	
	int parent1 = find_cluster_parent(start_cluster);
	int parent2 = find_cluster_parent(first_cluster);
	pthread_mutex_unlock(fat_lock);
	
	swap(parent1, parent2, start_cluster, curr);
	
	for(i = 1; i < count; i++) {
		curr = new_fat[start_cluster+i-1];
		pthread_mutex_lock(fat_lock);
		int parent = find_cluster_parent(start_cluster+i);
		pthread_mutex_unlock(fat_lock);
		swap(parent, start_cluster+i-1, start_cluster+i, curr);
	}
}

void *defrag_procedure(void *arg) {
	thread_data *td;
	int score = 0;
	while((td = get_job()) || factory->running) {
	    score++;
	    defrag(td->cluster_file, td->cluster_start, td->cluster_count, td->cluster_first);
	    //printf("job done! [%d - %d - %d]\n", td->cluster_start, td->cluster_first, td->cluster_count);
	    free(td);
	}
	printf("Dokoncena prace - score: %d\n", score*task_size);
}

void control(int file, int start, int count) {
	int i, j;
	for(i = 1; i < p_boot_record->fat_copies; i++) {
		int curr = start;
		for(j = 0; j < count; j++) {
			if(new_fat[curr] != fat_item[i][curr]) {
				printf("Chyba v souboru %d. FAT%d - cluster %d neodpovida FAT1\n", file+1, i+1, curr);
			}
			curr = new_fat[curr];
		}
	}
}

void *control_procedure(void *arg) {
	thread_data *td;
	int score = 0;
	while((td = get_job()) || factory->running) {
	    score++;
	    control(td->cluster_file, td->cluster_first, td->cluster_count);
	    //printf("job done! [%d - %d - %d]\n", td->cluster_file, td->cluster_first, td->cluster_count);
	    free(td);
	}
	printf("Dokoncena prace - score: %d\n", score*task_size);
}
