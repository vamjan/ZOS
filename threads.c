#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include "fat.h"
#include "threads.h"

//tento soubor obsahuje v�echny funkce pro paralelizaci �lohy

pthread_mutex_t **cluster_locks;	//pole obsahuj�c� mutexy cluster�
pthread_mutex_t *fat_lock;			//jeden mutex pro celu FAT tabulku
producer *factory;					//struktura obsahuj�c� data producenta

/**
 * Funkce uvoln�n� v�ech z�mk�
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
 * Funkce vytvo�en� vl�ken konzument�
 * parametry:	count - po�et vl�ken
 *				function - funkce, kterou budou vl�kna prov�d�t
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
 * Funkce pro ukon�en� vl�ken konzument�
 * parametry:	count - po�et vl�ken
 *				threads - pole vl�ken
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
 * Funkce pro vytvo�en� z�mk� cluster�
 * parametry:	count - po�et cluster�
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
 * Funkce pro vytvo�en� z�mk�
 */
void setup_locks() {
	cluster_locks = create_cluster_locks(p_boot_record->cluster_count);
	fat_lock = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(fat_lock, NULL);
}

/**
 * Funkce vyb�r�n� �loh ze spole�n�ho bufferu. Volan� konzumenty.
 */
thread_data *get_job() {
	thread_data *retval = malloc(sizeof(thread_data));
	sem_wait(&factory->filled);				//kritick� sekce: semafor-wait p�ipraven� �lohy
	pthread_mutex_lock(&factory->lock);		//kritick� sekce: zab�r�m factory mutex
	
	//v�b�r �lohy z bufferu pokud je n�jak� voln�, jinak NULL
	if(factory->buffer_index_out != factory->buffer_index_in ) {
		memcpy(retval, &factory->buffer[factory->buffer_index_out%BUFFER_SIZE], sizeof(thread_data));
		factory->buffer_index_out++;
	} else {
		retval = NULL;
	}
	
	pthread_mutex_unlock(&factory->lock);	//kritick� sekce: opou�t�m factory mutex
	sem_post(&factory->empty);				//kritick� sekce: semafor-post pr�zdn� �lohy
	return retval;
}

/**
 * Funkce p�id�v�n� �loh do spole�n�ho bufferu. Volan� producentem.
 */
void add_job(thread_data td) {
	sem_wait(&factory->empty);				//kritick� sekce: semafor-wait pr�zdn� �lohy
	pthread_mutex_lock(&factory->lock);		//kritick� sekce: zab�r�m factory mutex
	
	factory->buffer[factory->buffer_index_in%BUFFER_SIZE] = td;
    factory->buffer_index_in++;
    
    pthread_mutex_unlock(&factory->lock);	//kritick� sekce: opou�t�m factory mutex
    sem_post(&factory->filled);				//kritick� sekce: semafor-post p�ipraven� �lohy
}

/**
 * Funkce prov�d�n� producentem dokud neprojde cel� soubor. V t�to funcki vytv��� producent �lohy
 * o d�lce max TASK_SIZE cluster�. Pokud b�hem pr�chodu souborem naraz� na konec nebo neplatn� cluster
 * pak je d�lka �lohy krat��.
 * parametry:	count - po�et vl�ken
 *				function - flag co p�ep�n� mezi funkcemi (kontrola nebo defrag)
 */
void create_jobs(int count, int function) {
	int i, curr;
	thread_data td;
	//pr�chod po souborech, soubor se rozd�l� na v�ce �loh podle po�tu cluster�
    for(i = 0; i < p_boot_record->root_directory_max_entries_count; i++) {
    	curr = p_root_directory[i]->first_cluster;
    	//po�et �loh pro dan� soubor, po�et cluster� v aktu�ln� �loze
    	int job_count = 0, cluster_count = 0;
		td.cluster_first = curr;
    	
    	if(function) td.cluster_start = 0;
    	td.cluster_file = i;
    	
    	//pr�chod FAT tabulkou v dan�m souboru, v p��pad� konce nebo neplatn�ho z�znamu je
    	//ukon�en pr�chod st�vaj�c�m souborem a p�ech�z� se na dal��
    	while(curr != FAT_FILE_END && curr != FAT_UNUSED && curr != FAT_BAD_CLUSTER) {
			curr = new_fat[curr];
			//pokud projde dostate�n�m mno�stv�m cluster� tak vytvo�� novou �lohu a za�ne vytv��et dal��
			if(++cluster_count == TASK_SIZE) {
				td.cluster_count = cluster_count;
    			
    			add_job(td);
    			
    			job_count++;
    			
    			td.cluster_first = curr;
    			
    			//pro kontrolu konzistence neni pot�eba tento parametr nastavovat
    			if(function) td.cluster_start = cluster_count*job_count;
    			td.cluster_file = i;
    			
				cluster_count = 0;
    		}
    	}
    	td.cluster_count = cluster_count;
    	cluster_count = 0;
    	//p�id�n� posledn� �lohy pokud nen� pr�zdn�
    	if(td.cluster_count != 0) {
	    	add_job(td);
	    }
    }
    //vypnut� producenta
    factory->running = 0;
    //uvoln�n� v�ech vl�ken �ekaj�c�ch na semaforu (u� nedostanou �lohy)
    for(i = 0; i < count; i++) {
    	sem_post(&factory->filled);
    }
}

/**
 * Funkce nastaven� producenta.
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
 * Funkce synchronizovan�ho prohozen� 2 cluster�. P�esune data a z�znamy FAT.
 * parametry:	point1 - index ukazatele na cluster1
 *				point2 - index ukazatele na cluster2
 *				cluster1 - index FAT z�znamu cluster1
 *				cluster2 - index FAT z�znamu cluster2
 */
void swap(int point1, int point2, int cluster1, int cluster2) {
	int i;
	//o�et�en� vstup�
	if(cluster1 >= p_boot_record->cluster_count || cluster2 >= p_boot_record->cluster_count) return;
    if(cluster1 == cluster2) { return; }
    
    pthread_mutex_lock(fat_lock);					//kritick� sekce: zab�r�m fat mutex
    swap_fat(point1, point2, cluster1, cluster2);
    pthread_mutex_unlock(fat_lock);					//kritick� sekce: opou�t�m fat mutex
    
    pthread_mutex_lock(cluster_locks[cluster1]);	//kritick� sekce: zab�r�m cluster1 mutex
    pthread_mutex_lock(cluster_locks[cluster2]);	//kritick� sekce: zab�r�m cluster2 mutex
    swap_clusters(cluster1, cluster2);
    pthread_mutex_unlock(cluster_locks[cluster2]);	//kritick� sekce: opou�t�m cluster2 mutex
    pthread_mutex_unlock(cluster_locks[cluster1]);	//kritick� sekce: opou�t�m cluster1 mutex
    
    pthread_mutex_lock(fat_lock);					//kritick� sekce: zab�r�m fat mutex
    //zm�na po��te�n�ho root_directory clusteru pokud do�lo k jeho prohozen�
    for(i = 0; i < p_boot_record->root_directory_max_entries_count; i++) {
		if(cluster1 == p_root_directory[i]->first_cluster)
			p_root_directory[i]->first_cluster = cluster2;
		else if(cluster2 == p_root_directory[i]->first_cluster)
			p_root_directory[i]->first_cluster = cluster1;
	}
	pthread_mutex_unlock(fat_lock);					//kritick� sekce: opou�t�m fat mutex
}

/**
 * Funkce defragmentace volan� z procedury vl�kna. P�eskupuje clustery tak, aby �li za sebou.
 * Pr�zdn� m�sto potom bude na konci souboru.
 * parametry:	file - index souboru
 *				start - index spr�vn� pozice prvn�ho clusteru
 *				count - po�et prohazovan�ch cluster� (velikost �lohy)
 *				first_cluster - aktu�ln� index prvn�ho clusteru
 */
void defrag(int file, int start, int count, int first_cluster) {
	int i, curr = first_cluster, start_cluster = correct_first_cluster(file, start);
	
	pthread_mutex_lock(fat_lock);			//kritick� sekce: zab�r�m fat mutex
	//najdu sou�adnice
	int parent1 = find_cluster_parent(start_cluster);
	int parent2 = find_cluster_parent(first_cluster);
	pthread_mutex_unlock(fat_lock);			//kritick� sekce: opou�t�m fat mutex
	
	//prohozen� prvn�ho clusteru
	swap(parent1, parent2, start_cluster, curr);
	
	for(i = 1; i < count; i++) {
		//posunut� na dal�� cluster
		curr = new_fat[start_cluster+i-1];
		pthread_mutex_lock(fat_lock);		//kritick� sekce: zab�r�m fat mutex
		int parent = find_cluster_parent(start_cluster+i);
		pthread_mutex_unlock(fat_lock);		//kritick� sekce: opou�t�m fat mutex
		//prohozen� cluster�
		swap(parent, start_cluster+i-1, start_cluster+i, curr);
	}
}

/**
 * Procedura defragmentace vl�kna. Tuto funkci dostane vl�kno p�i jeho vytv��en�.
 * parametry:	arg - nic
 */
void *defrag_procedure(void *arg) {
	thread_data *td;
	//navratova hodnota vl�kna
	int *score = malloc(sizeof(int));
	*score = 0;
	//vl�kno ukon�� �innost puze pokud nedostane pr�ci a producent u� nevytv��� dal��
	while((td = get_job()) || factory->running) {
	    (*score)++;
	    defrag(td->cluster_file, td->cluster_start, td->cluster_count, td->cluster_first);
	    free(td);
	}
	return (void *)score;
}

/**
 * Funkce kontroly konzistence volan� z procedury vl�kna. Funkce proch�z� clustery po souborech.
 * (pomoc� FAT tabulky) a hled� nesrovnalosti v ostatn�ch FAT tabulk�ch.
 * parametry:	file - index souboru
 *				start - aktu�ln� index prvn�ho clusteru
 *				count - po�et prohazovan�ch cluster� (velikost �lohy)
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
 * Procedura kontroly konzistence vl�kna. Tuto funkci dostane vl�kno p�i jeho vytv��en�.
 * parametry:	arg - nic
 */
void *control_procedure(void *arg) {
	thread_data *td;
	//n�vratov� hodnota vl�kna
	int *score = malloc(sizeof(int));
	*score = 0;
	//vl�kno ukon�� �innost puze pokud nedostane pr�ci a producent u� nevytv��� dal��
	while((td = get_job()) || factory->running) {
	    (*score)++;
	    control(td->cluster_file, td->cluster_first, td->cluster_count);
	    free(td);
	}
	return (void *)score;
}
