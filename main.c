#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include "fat.h"

/* run this program using the console pauser or add your own getch, system("pause") or input loop */
void help() {
	printf("Program pro defragmentaci souboru a kontrolu konzistence FAT.\n");
	printf("Spouštìní: ./zos (parametr) (soubor) (poèet vláken)\n");
	printf("Pomocí parametrù je možné urèit funkci. Možné parametry jsou:\n");
	printf("h - help\n");
	printf("d - defragment\n");
	printf("c - kontrola FAT\n");
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
	
	load_file("output.fat");           
   
    return 0;
}
