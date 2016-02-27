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
thread_data **jobs;
producer *factory;

void free_locks() {
	
}

pthread_t **create_threads(int count, void *(*function)(void *)) {
	int i;
	pthread_t **retval = malloc(sizeof(pthread_t *) * count);
	for(i = 0; i < count; i++) {
		retval[i] = malloc(sizeof(pthread_t));
		pthread_create(retval[i], NULL, function, NULL);
		printf("Thread %d status: alive and kickin\'\n", i);
	}
	
	return retval;
}

int join_threads(int count, pthread_t **threads) {
	int i;
	for(i = 0; i < count; i++) {
		pthread_join(*threads[i], NULL);
		printf("Thread %d status: REKT\n", i);
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
	printf("All locks are ready to go . . .\n");
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

void defrag(int start, int first_cluster, int count) {
	int i, curr = first_cluster;
	//printf("swapuju %d %d --- %d %d --- %s\n", find_cluster_parent(start), find_cluster_parent(first_cluster), start, curr, clusters[curr]);
	
	pthread_mutex_lock(fat_lock);	
	int parent1 = find_cluster_parent(start);
	int parent2 = find_cluster_parent(first_cluster);
	pthread_mutex_unlock(fat_lock);
	
	swap(parent1, parent2, start, curr);
	
	for(i = 1; i < count; i++) {
		curr = new_fat[start+i-1];
		pthread_mutex_lock(fat_lock);
		int parent = find_cluster_parent(start+i);
		pthread_mutex_unlock(fat_lock);
		//printf("swapuju %d %d --- %d %d --- %s\n", parent, start+i-1, start+i, curr, clusters[curr]); 
		swap(parent, start+i-1, start+i, curr);
	}
}

void *consumer_procedure(void *arg) {
	thread_data td;
	int score = 0;
	while(factory->buffer_index_out != factory->buffer_index_in || factory->running) {
		sem_wait(&factory->filled);
		pthread_mutex_lock(&factory->lock);
		
		td = factory->buffer[factory->buffer_index_out%BUFFER_SIZE];
	    printf("job %d taken! [%d - %d - %d]\n", factory->buffer_index_out, td.cluster_start, td.cluster_first, td.cluster_count);
		factory->buffer_index_out++;
	    score++;
	    
	    pthread_mutex_unlock(&factory->lock);
	    sem_post(&factory->empty);
	    
	    defrag(td.cluster_start, td.cluster_first, td.cluster_count);
	}
	printf("nemam praci - score: %d\n", score);
}

void *control_procedure(void *arg) {
	
}

void add_job(thread_data td) {
	sem_wait(&factory->empty);
	pthread_mutex_lock(&factory->lock);
	
	factory->buffer[factory->buffer_index_in%BUFFER_SIZE] = td;
    printf("job %d created! [%d - %d - %d]\n", factory->buffer_index_in, td.cluster_start, td.cluster_first, td.cluster_count);
    factory->buffer_index_in++;
    
    pthread_mutex_unlock(&factory->lock);
    sem_post(&factory->filled);
}

void create_jobs(int lenght) {
	int i, curr;
	thread_data td;
	printf("Producer starting. . .\n");
    for(i = 0; i < p_boot_record->root_directory_max_entries_count; i++) {
    	curr = p_root_directory[i]->first_cluster;
    	int job_count = 0, cluster_count = 0;
    	td.cluster_first = curr;
    	td.cluster_start = correct_first_cluster(i, 0);
    	while(curr != FAT_FILE_END) {
			curr = new_fat[curr];
			if(++cluster_count == lenght) {
				td.cluster_count = cluster_count;
    			
    			add_job(td);
    			
    			job_count++;
    			td.cluster_first = curr;
    			td.cluster_start = correct_first_cluster(i, cluster_count*job_count);
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
    printf("Producer done. . .\n");
}

void setup_producer() {
	factory = malloc(sizeof(producer));
	factory->running = 1;
	factory->buffer_index_in = 0;
	factory->buffer_index_out = 0;
	sem_init(&factory->filled, 0, 0);
    sem_init(&factory->empty, 0, BUFFER_SIZE);
    pthread_mutex_init(&factory->lock, NULL);
    printf("Producer ready. . .\n");
}
