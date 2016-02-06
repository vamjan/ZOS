#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include "fat.h"
#define THREADS 5

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

void *Procedure(void *arg) {
	int tmp = (int *)arg;
	int i;
	
	for(i = 0; i < THREADS; i++) {
		A[i*THREADS+tmp] = tmp+1;
	}
	
	printf("Vlakno c.%d ukoncuje praci. Zapsano %d cisel.\n", tmp, i);
	
	return &i;
}

int main(int argc, char* argv[]) {
	if(argc == 2 && argv[1][0] == 'h') {
		help();
		exit(0);
	}
	
	if(argc != 4) {
		printf("Neplatne parametry!");
		help();
		exit(0);
	}
	
	//load_file("output.fat");
	
	int i;
	void *retval;
	
	for(i = 0; i < THREADS; i++) {
		pthread_create(th[i], NULL, Procedure, i);
	}
	
	for(i = 0; i < THREADS; i++) {
		pthread_join(th[i], &retval);
		printf("Pocet zapsanych cisel vlaknem c.%d je: %d\n", i+1, (int *)retval);
	}
	
	printf("Vysledek\n");
	for(i = 0; i < THREADS*THREADS; i++) {
		printf("%d, ", A[i]);
	}
   
    return 0;
}

