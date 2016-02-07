#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include "fat.h"
#define THREADS 10

int A[THREADS*THREADS];
pthread_t th[THREADS];

/* run this program using the console pauser or add your own getch, system("pause") or input loop */
void help() {
	printf("Program pro defragmentaci souboru a kontrolu konzistence FAT.\n");
	printf("Spouštìní: ./zos (parametr) (soubor) (poèet vláken)\n");
	printf("Pomocí parametrù je možné urèit funkci. Možné parametry jsou:\n");
	printf("h - help\n");
	printf("d - defragment\n");
	printf("c - kontrola FAT\n");
}

void run_defrag(int threads) {
	load_file();
	print_clusters();
	swap_clusters(0, 1);
	print_clusters();
	swap_clusters(0, 1);
	print_clusters();
}

void run_con_check(int threads) {
	
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
	
	if(open_file(argv[2])) {
		printf("Soubor %s uspesne otevren!\n", argv[2]);
	} else {
		printf("Chyba pri otevirani souboru.\n");
		exit(2);
	}
	
	threads = atoi(argv[3]);
	if(threads < 1 || threads > 100) {
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
   
    return 0;
}

