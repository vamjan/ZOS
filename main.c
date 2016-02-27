#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include "threads.h"
#include "fat.h"

#define TASK_SIZE 25

pthread_t **consumer_threads;

void help() {
	printf("Program pro defragmentaci souboru a kontrolu konzistence FAT.\n");
	printf("Spousteni: ./zos (parametr) (soubor) (pocet vlaken)\n");
	printf("Pomoci parametru je mozne urcit funkci. Mozne parametry jsou:\n");
	printf("h - help\n");
	printf("d - defragment\n");
	printf("c - kontrola FAT\n");
}

void free_threads(int threads) {
	int i;
	for(i = 0; i < threads; i++) {
		free(consumer_threads[i]);
	}
	free(consumer_threads);
}

void run_defrag(int threads) {
	load_file();
	load_clusters();
	
	setup_locks(threads);
	setup_producer();
	
	consumer_threads = create_threads(threads, &defrag_procedure);
	
	struct timespec start, stop;
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);
	
	create_jobs(TASK_SIZE, threads, 1);
	
	join_threads(threads, consumer_threads);
	
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &stop);
	printf("Doba vypoctu: %lf\n", (stop.tv_sec - start.tv_sec) * 1e6 + (stop.tv_nsec - start.tv_nsec) / 1e3);
	
    write_result();
}

void run_con_check(int threads) {
	load_file();
	
	setup_locks(threads);
	setup_producer();
	
	consumer_threads = create_threads(threads, &control_procedure);
	
	struct timespec start, stop;
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);
	
	create_jobs(TASK_SIZE, threads, 0);
	
	join_threads(threads, consumer_threads);
	
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &stop);
	printf("Doba vypoctu: %lf\n", (stop.tv_sec - start.tv_sec) * 1e6 + (stop.tv_nsec - start.tv_nsec) / 1e3);
}

int main(int argc, char* argv[]) {
	
	int threads;
	
	if(argc == 2 && argv[1][0] == 'h') {
		help();
		exit(0);
	}
	
	if(argc != 4) {
		printf("Neplatne parametry!");
		help();
		exit(1);
	}
	
	if(!strstr(argv[2], ".fat")) {
		printf("Soubor ma nespravny format.\n");
		exit(2);
	}
	
	if(open_file(argv[2])) {
		printf("Soubor %s uspesne otevren!\n", argv[2]);
	} else {
		printf("Chyba pri otevirani souboru.\n");
		exit(2);
	}
	
	threads = atoi(argv[3]);
	if(threads < 1 || threads > 32) {
		printf("Zadany pocet vlaken je neplatny! Program bude pokracovat jedinym vlaknem.\n");
		threads = 1;
	}
	
	if(argv[1][0] == 'd') {
		printf("Zacina defragmentace . . .\n");
		run_defrag(threads);
	} else if(argv[1][0] == 'c') {
		printf("Zacina kontrola konzistence . . .\n");
		run_con_check(threads);
	} else {
		printf("Zadany parametr funkce je neplatny!\n");
		exit(1);
	}
	
	free_threads(threads);
    free_locks();
    free_data();
   
    return 0;
}

