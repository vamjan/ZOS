#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include "fat.h"

/* run this program using the console pauser or add your own getch, system("pause") or input loop */
void help() {
	printf("Program pro defragmentaci souboru a kontrolu konzistence FAT.\n");
	printf("Spou�t�n�: ./zos (parametr) (soubor) (po�et vl�ken)\n");
	printf("Pomoc� parametr� je mo�n� ur�it funkci. Mo�n� parametry jsou:\n");
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
