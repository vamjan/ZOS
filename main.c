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
	printf("Spou�t�n�: ./zos (parametr) (soubor) (po�et vl�ken)\n");
	printf("Pomoc� parametr� je mo�n� ur�it funkci. Mo�n� parametry jsou:\n");
	printf("h - help\n");
	printf("d - defragment\n");
	printf("c - kontrola FAT\n");
}

void *Procedure(void *arg) {
	int tmp = (int *)arg;
	int i;
	int *retval;
	
	for(i = 0; i < THREADS; i++) {
		A[i*THREADS+tmp] = tmp+1;
	}
	
	return (void *) i;
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
	
	printf("Starting!\n");
	
	int i;
	void *retval;
	
	for(i = 0; i < THREADS; i++) {
		printf("Vytvarim vlakno c.%d\n", i);
		pthread_create(&th[i], NULL, Procedure, i);
	}
	
	for(i = 0; i < THREADS; i++) {
		pthread_join(th[i], &retval);
		printf("Pocet zapsanych cisel vlaknem c.%d je: %d\n", i, (int)retval);
	}
	
	printf("Vysledek\n");
	for(i = 0; i < THREADS; i++) {
		printf("%d, ", A[i]);
	}
   
    return 0;
}

