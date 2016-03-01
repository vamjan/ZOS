#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include "threads.h"
#include "fat.h"

//Program pro simulaci defregmentace a kontroly konzistence FAT souboru. Vytvo�eno v r�mci p�edm�tu KIV/ZOS na Z�U.
//autor: Jan Vampol

pthread_t **consumer_threads;	//pole vl�ken knzument�


/**
 * Funkce pro v�pis n�pov�dy v p��pad� zad�n� neplatn�ch parametr�
 */
void help() {
	printf("Program pro defragmentaci souboru a kontrolu konzistence FAT.\n");
	printf("Spousteni: ./zos (parametr) (soubor) (pocet vlaken)\n");
	printf("Pomoci parametru je mozne urcit funkci. Mozne parametry jsou:\n");
	printf("h - help\n");
	printf("d - defragment\n");
	printf("c - kontrola FAT\n");
}

/**
 * Funkce uvoln�n� vl�ken
 */
void free_threads(int threads) {
	int i;
	for(i = 0; i < threads; i++) {
		free(consumer_threads[i]);
	}
	free(consumer_threads);
}

/**
 * Funkce defragmentace.
 * Na za��tu na�te hlavi�ku souboru, FAT tabulky a obsah cluster� do pam�ti.
 * Potom prob�hne nastaven� producenta, konzument� a z�mk�.
 * Spu�t�n� konzument za�ne vytv��et �koly o velikosti TASK_SIZE.
 * Vl�kna si p�eb�raj� �koly p�es spole�n� buffer a prov�d� funkci, kter� jim byla p�ed�na (defragmentace).
 * Na konec se c�lov� soubor p�ep�e nov�mi ji� defragmentovan�mi daty.
 */
void run_defrag(int threads) {
	clock_t start, end;
	
	load_file();
	load_clusters();
	
	setup_locks();
	setup_producer();
	
	consumer_threads = create_threads(threads, &defrag_procedure);
	
	//m��en� �asu b�hu vl�ken, vyu��v� se <time.h>
	start = clock();
	
	create_jobs(threads, 1);
	
	join_threads(threads, consumer_threads);
	
	//v�sledn� �as je v CPU time - mus� se p�ev�st na sekundy
	end = clock();
	printf("Doba vypoctu (paralelni): %fs\n", (end - start)/(double)CLOCKS_PER_SEC);
	
    write_result();
}

/**
 * Funkce kontroly konzistence.
 * Na za��tu na�te hlavi�ku souboru a FAT tabulky do pam�ti.
 * Potom prob�hne nastaven� producenta, konzument� a z�mk�.
 * Spu�t�n� konzument za�ne vytv��et �koly o velikosti TASK_SIZE.
 * Vl�kna si p�eb�raj� �koly p�es spole�n� buffer a prov�d� funkci, kter� jim byla p�ed�na (kontrola konzistence).
 * V�stupem je v�pis potencion�ln� po�kozen�ch soubor�
 */
void run_con_check(int threads) {
	clock_t start, end;
	
	load_file();
	
	setup_locks();
	setup_producer();
	
	consumer_threads = create_threads(threads, &control_procedure);
	
	//m��en� �asu b�hu vl�ken, vyu��v� se <time.h>
	start = clock();
	
	create_jobs(threads, 0);
	
	join_threads(threads, consumer_threads);
	
	//v�sledn� �as je v CPU time - mus� se p�ev�st na sekundy
	end = clock();
	printf("Doba vypoctu (paralelni): %fs\n", (end - start)/(double)CLOCKS_PER_SEC);
}

/**
 * Hlavn� funkce main
 */
int main(int argc, char* argv[]) {
	clock_t start, end;
	int threads;
	
	start = clock();
	
	//kontrola po�tu parametr� i pro p��pad vol�n� pomoci (h - help)
	if(argc == 2 && argv[1][0] == 'h') {
		help();
		exit(0);
	}
	
	if(argc != 4) {
		printf("Neplatne parametry!");
		help();
		exit(1);
	}
	
	//kontrola spr�vn�ho form�tu souboru
	if(!strstr(argv[2], ".fat")) {
		printf("Soubor ma nespravny format.\n");
		exit(2);
	}
	
	//pokus o otevreni souboru
	if(open_file(argv[2])) {
		//printf("Soubor %s uspesne otevren!\n", argv[2]);
	} else {
		printf("Chyba pri otevirani souboru.\n");
		exit(2);
	}
	
	//kontrola spr�vn�ho zad�n� po�tu vl�ken
	threads = atoi(argv[3]);
	if(threads < 1 || threads > 32) {
		printf("Zadany pocet vlaken je neplatny! Program bude pokracovat jedinym vlaknem.\n");
		threads = 1;
	}
	
	//zji�t�n� po�adovan� funkce a jej� vol�n�
	if(argv[1][0] == 'd') {
		//printf("Zacina defragmentace . . .\n");
		run_defrag(threads);
	} else if(argv[1][0] == 'c') {
		//printf("Zacina kontrola konzistence . . .\n");
		run_con_check(threads);
	} else {
		printf("Zadany parametr funkce je neplatny!\n");
		exit(1);
	}
	
	//�klid
	free_threads(threads);
    free_locks();
    free_data();
    
    end = clock();
	printf("Doba vypoctu (celkova): %fs\n", (end - start)/(double)CLOCKS_PER_SEC);
   
    return 0;
}

