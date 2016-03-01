#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include "fat.h"
#include "threads.h"

//tento soubor obsahuje všechny funkce pro paralelizaci úlohy

pthread_mutex_t **cluster_locks;	//pole obsahující mutexy clusterù
pthread_mutex_t *fat_lock;			//jeden mutex pro celu FAT tabulku
producer *factory;					//struktura obsahující data producenta

/**
 * Funkce uvolnìní všech zámkù
 */
void free_locks() {
	int i;
	
	for(i = 0; i < p_boot_record->cluster_count; i++) {
		free(cluster_locks[i]);
	}
	
	free(cluster_locks);
	free(fat_lock);
	free(factory);
}

/**
 * Funkce vytvoøení vláken konzumentù
 * parametry:	count - poèet vláken
 *				function - funkce, kterou budou vlákna provádìt
 */
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

/**
 * Funkce pro ukonèení vláken konzumentù
 * parametry:	count - poèet vláken
 *				threads - pole vláken
 */
int join_threads(int count, pthread_t **threads) {
	int i;
	void *retval;
	for(i = 0; i < count; i++) {
		pthread_join(*threads[i], &retval);
		printf("Thread %d status: KONCIM - zpracovano: %d\n", i, *(int *)retval);
	}
}

/**
 * Funkce pro vytvoøení zámkù clusterù
 * parametry:	count - poèet clusterù
 */
pthread_mutex_t **create_cluster_locks(int count) {
	int i;
	pthread_mutex_t **retval = malloc(sizeof(pthread_mutex_t *) * count);
	for(i = 0; i < count; i++) {
		retval[i] = malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(retval[i], NULL);
	}
	
	return retval;
}

/**
 * Funkce pro vytvoøení zámkù
 */
void setup_locks() {
	cluster_locks = create_cluster_locks(p_boot_record->cluster_count);
	fat_lock = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(fat_lock, NULL);
}

/**
 * Funkce vybírání úloh ze spoleèného bufferu. Volaná konzumenty.
 */
thread_data *get_job() {
	thread_data *retval = malloc(sizeof(thread_data));
	sem_wait(&factory->filled);				//kritická sekce: semafor-wait pøipravené úlohy
	pthread_mutex_lock(&factory->lock);		//kritická sekce: zabírám factory mutex
	
	//výbìr úlohy z bufferu pokud je nìjaká volná, jinak NULL
	if(factory->buffer_index_out != factory->buffer_index_in ) {
		memcpy(retval, &factory->buffer[factory->buffer_index_out%BUFFER_SIZE], sizeof(thread_data));
		factory->buffer_index_out++;
	} else {
		retval = NULL;
	}
	
	pthread_mutex_unlock(&factory->lock);	//kritická sekce: opouštím factory mutex
	sem_post(&factory->empty);				//kritická sekce: semafor-post prázdné úlohy
	return retval;
}

/**
 * Funkce pøidávání úloh do spoleèného bufferu. Volaná producentem.
 */
void add_job(thread_data td) {
	sem_wait(&factory->empty);				//kritická sekce: semafor-wait prázdné úlohy
	pthread_mutex_lock(&factory->lock);		//kritická sekce: zabírám factory mutex
	
	factory->buffer[factory->buffer_index_in%BUFFER_SIZE] = td;
    factory->buffer_index_in++;
    
    pthread_mutex_unlock(&factory->lock);	//kritická sekce: opouštím factory mutex
    sem_post(&factory->filled);				//kritická sekce: semafor-post pøipravené úlohy
}

/**
 * Funkce provádìná producentem dokud neprojde celý soubor. V této funcki vytváøí producent úlohy
 * o délce max TASK_SIZE clusterù. Pokud bìhem prùchodu souborem narazí na konec nebo neplatný cluster
 * pak je délka úlohy kratší.
 * parametry:	count - poèet vláken
 *				function - flag co pøepíná mezi funkcemi (kontrola nebo defrag)
 */
void create_jobs(int count, int function) {
	int i, curr;
	thread_data td;
	//prùchod po souborech, soubor se rozdìlí na více úloh podle poètu clusterù
    for(i = 0; i < p_boot_record->root_directory_max_entries_count; i++) {
    	curr = p_root_directory[i]->first_cluster;
    	//poèet úloh pro daný soubor, poèet clusterù v aktuální úloze
    	int job_count = 0, cluster_count = 0;
		td.cluster_first = curr;
    	
    	if(function) td.cluster_start = 0;
    	td.cluster_file = i;
    	
    	//prùchod FAT tabulkou v daném souboru, v pøípadì konce nebo neplatného záznamu je
    	//ukonèen prùchod stávajícím souborem a pøechází se na další
    	while(curr != FAT_FILE_END && curr != FAT_UNUSED && curr != FAT_BAD_CLUSTER) {
			curr = new_fat[curr];
			//pokud projde dostateèným množstvím clusterù tak vytvoøí novou úlohu a zaène vytváøet další
			if(++cluster_count == TASK_SIZE) {
				td.cluster_count = cluster_count;
    			
    			add_job(td);
    			
    			job_count++;
    			
    			td.cluster_first = curr;
    			
    			//pro kontrolu konzistence neni potøeba tento parametr nastavovat
    			if(function) td.cluster_start = cluster_count*job_count;
    			td.cluster_file = i;
    			
				cluster_count = 0;
    		}
    	}
    	td.cluster_count = cluster_count;
    	cluster_count = 0;
    	//pøidání poslední úlohy pokud není prázdná
    	if(td.cluster_count != 0) {
	    	add_job(td);
	    }
    }
    //vypnutí producenta
    factory->running = 0;
    //uvolnìní všech vláken èekajících na semaforu (už nedostanou úlohy)
    for(i = 0; i < count; i++) {
    	sem_post(&factory->filled);
    }
}

/**
 * Funkce nastavení producenta.
 */
void setup_producer() {
	factory = malloc(sizeof(producer));
	factory->running = 1;
	factory->buffer_index_in = 0;
	factory->buffer_index_out = 0;
	sem_init(&factory->filled, 0, 0);
    sem_init(&factory->empty, 0, BUFFER_SIZE);
    pthread_mutex_init(&factory->lock, NULL);
}

/**
 * Funkce synchronizovaného prohození 2 clusterù. Pøesune data a záznamy FAT.
 * parametry:	point1 - index ukazatele na cluster1
 *				point2 - index ukazatele na cluster2
 *				cluster1 - index FAT záznamu cluster1
 *				cluster2 - index FAT záznamu cluster2
 */
void swap(int point1, int point2, int cluster1, int cluster2) {
	int i;
	//ošetøení vstupù
	if(cluster1 >= p_boot_record->cluster_count || cluster2 >= p_boot_record->cluster_count) return;
    if(cluster1 == cluster2) { return; }
    
    pthread_mutex_lock(fat_lock);					//kritická sekce: zabírám fat mutex
    swap_fat(point1, point2, cluster1, cluster2);
    pthread_mutex_unlock(fat_lock);					//kritická sekce: opouštím fat mutex
    
    pthread_mutex_lock(cluster_locks[cluster1]);	//kritická sekce: zabírám cluster1 mutex
    pthread_mutex_lock(cluster_locks[cluster2]);	//kritická sekce: zabírám cluster2 mutex
    swap_clusters(cluster1, cluster2);
    pthread_mutex_unlock(cluster_locks[cluster2]);	//kritická sekce: opouštím cluster2 mutex
    pthread_mutex_unlock(cluster_locks[cluster1]);	//kritická sekce: opouštím cluster1 mutex
    
    pthread_mutex_lock(fat_lock);					//kritická sekce: zabírám fat mutex
    //zmìna poèáteèního root_directory clusteru pokud došlo k jeho prohození
    for(i = 0; i < p_boot_record->root_directory_max_entries_count; i++) {
		if(cluster1 == p_root_directory[i]->first_cluster)
			p_root_directory[i]->first_cluster = cluster2;
		else if(cluster2 == p_root_directory[i]->first_cluster)
			p_root_directory[i]->first_cluster = cluster1;
	}
	pthread_mutex_unlock(fat_lock);					//kritická sekce: opouštím fat mutex
}

/**
 * Funkce defragmentace volaná z procedury vlákna. Pøeskupuje clustery tak, aby šli za sebou.
 * Prázdné místo potom bude na konci souboru.
 * parametry:	file - index souboru
 *				start - index správné pozice prvního clusteru
 *				count - poèet prohazovaných clusterù (velikost úlohy)
 *				first_cluster - aktuální index prvního clusteru
 */
void defrag(int file, int start, int count, int first_cluster) {
	int i, curr = first_cluster, start_cluster = correct_first_cluster(file, start);
	
	pthread_mutex_lock(fat_lock);			//kritická sekce: zabírám fat mutex
	//najdu souøadnice
	int parent1 = find_cluster_parent(start_cluster);
	int parent2 = find_cluster_parent(first_cluster);
	pthread_mutex_unlock(fat_lock);			//kritická sekce: opouštím fat mutex
	
	//prohození prvního clusteru
	swap(parent1, parent2, start_cluster, curr);
	
	for(i = 1; i < count; i++) {
		//posunutí na další cluster
		curr = new_fat[start_cluster+i-1];
		pthread_mutex_lock(fat_lock);		//kritická sekce: zabírám fat mutex
		int parent = find_cluster_parent(start_cluster+i);
		pthread_mutex_unlock(fat_lock);		//kritická sekce: opouštím fat mutex
		//prohození clusterù
		swap(parent, start_cluster+i-1, start_cluster+i, curr);
	}
}

/**
 * Procedura defragmentace vlákna. Tuto funkci dostane vlákno pøi jeho vytváøení.
 * parametry:	arg - nic
 */
void *defrag_procedure(void *arg) {
	thread_data *td;
	//navratova hodnota vlákna
	int *score = malloc(sizeof(int));
	*score = 0;
	//vlákno ukonèí èinnost puze pokud nedostane práci a producent už nevytváøí další
	while((td = get_job()) || factory->running) {
	    (*score)++;
	    defrag(td->cluster_file, td->cluster_start, td->cluster_count, td->cluster_first);
	    free(td);
	}
	return (void *)score;
}

/**
 * Funkce kontroly konzistence volaná z procedury vlákna. Funkce prochází clustery po souborech.
 * (pomocí FAT tabulky) a hledá nesrovnalosti v ostatních FAT tabulkách.
 * parametry:	file - index souboru
 *				start - aktuální index prvního clusteru
 *				count - poèet prohazovaných clusterù (velikost úlohy)
 */
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

/**
 * Procedura kontroly konzistence vlákna. Tuto funkci dostane vlákno pøi jeho vytváøení.
 * parametry:	arg - nic
 */
void *control_procedure(void *arg) {
	thread_data *td;
	//návratová hodnota vlákna
	int *score = malloc(sizeof(int));
	*score = 0;
	//vlákno ukonèí èinnost puze pokud nedostane práci a producent už nevytváøí další
	while((td = get_job()) || factory->running) {
	    (*score)++;
	    control(td->cluster_file, td->cluster_first, td->cluster_count);
	    free(td);
	}
	return (void *)score;
}
