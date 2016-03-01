#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include "threads.h"
#include "fat.h"

//Program pro simulaci defregmentace a kontroly konzistence FAT souboru. Vytvoøeno v rámci pøedmìtu KIV/ZOS na ZÈU.
//autor: Jan Vampol

pthread_t **consumer_threads;	//pole vláken knzumentù


/**
 * Funkce pro výpis nápovìdy v pøípadì zadání neplatných parametrù
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
 * Funkce uvolnìní vláken
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
 * Na zaèátu naète hlavièku souboru, FAT tabulky a obsah clusterù do pamìti.
 * Potom probìhne nastavení producenta, konzumentù a zámkù.
 * Spuštìný konzument zaène vytváøet úkoly o velikosti TASK_SIZE.
 * Vlákna si pøebírají úkoly pøes spoleèný buffer a provádí funkci, která jim byla pøedána (defragmentace).
 * Na konec se cílový soubor pøepíše novými již defragmentovanými daty.
 */
void run_defrag(int threads) {
	clock_t start, end;
	
	load_file();
	load_clusters();
	
	setup_locks();
	setup_producer();
	
	consumer_threads = create_threads(threads, &defrag_procedure);
	
	//mìøení èasu bìhu vláken, využívá se <time.h>
	start = clock();
	
	create_jobs(threads, 1);
	
	join_threads(threads, consumer_threads);
	
	//výsledný èas je v CPU time - musí se pøevést na sekundy
	end = clock();
	printf("Doba vypoctu (paralelni): %fs\n", (end - start)/(double)CLOCKS_PER_SEC);
	
    write_result();
}

/**
 * Funkce kontroly konzistence.
 * Na zaèátu naète hlavièku souboru a FAT tabulky do pamìti.
 * Potom probìhne nastavení producenta, konzumentù a zámkù.
 * Spuštìný konzument zaène vytváøet úkoly o velikosti TASK_SIZE.
 * Vlákna si pøebírají úkoly pøes spoleèný buffer a provádí funkci, která jim byla pøedána (kontrola konzistence).
 * Výstupem je výpis potencionálnì poškozených souborù
 */
void run_con_check(int threads) {
	clock_t start, end;
	
	load_file();
	
	setup_locks();
	setup_producer();
	
	consumer_threads = create_threads(threads, &control_procedure);
	
	//mìøení èasu bìhu vláken, využívá se <time.h>
	start = clock();
	
	create_jobs(threads, 0);
	
	join_threads(threads, consumer_threads);
	
	//výsledný èas je v CPU time - musí se pøevést na sekundy
	end = clock();
	printf("Doba vypoctu (paralelni): %fs\n", (end - start)/(double)CLOCKS_PER_SEC);
}

/**
 * Hlavní funkce main
 */
int main(int argc, char* argv[]) {
	clock_t start, end;
	int threads;
	
	start = clock();
	
	//kontrola poètu parametrù i pro pøípad volání pomoci (h - help)
	if(argc == 2 && argv[1][0] == 'h') {
		help();
		exit(0);
	}
	
	if(argc != 4) {
		printf("Neplatne parametry!");
		help();
		exit(1);
	}
	
	//kontrola správného formátu souboru
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
	
	//kontrola správného zadání poètu vláken
	threads = atoi(argv[3]);
	if(threads < 1 || threads > 32) {
		printf("Zadany pocet vlaken je neplatny! Program bude pokracovat jedinym vlaknem.\n");
		threads = 1;
	}
	
	//zjištìní požadované funkce a její volání
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
	
	//úklid
	free_threads(threads);
    free_locks();
    free_data();
    
    end = clock();
	printf("Doba vypoctu (celkova): %fs\n", (end - start)/(double)CLOCKS_PER_SEC);
   
    return 0;
}

