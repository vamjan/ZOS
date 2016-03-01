#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include "fat.h"

//tento soubor obsahuje všechny funkce pro práci s FAT tabulkami a clustery

FILE *p_file;						//FAT soubor, nad kterým se provádí požadovaná funkce
boot_record *p_boot_record;			//hlavièka FAT souboru obsahující informace
root_directory **p_root_directory;	//pole o velikosti root_directory_max_entries_count, jsou v nem ulozeny inforamce o souborech
unsigned int **fat_item, *new_fat;	//pole o velikosti cluster_count, reprezetuje puvodni FAT tabulky - - - nová FAT tabulka co se bude mìnit pøi výpoètu
char **clusters;					//pole clusterù kam se naète obsah souboru

/**
 * Funkce uvolnìní všech naètených dat
 */
void free_data() {
	int i;
	for(i = 0; i < p_boot_record->root_directory_max_entries_count; i++) {
		free(p_root_directory[i]);
	}
	
	for(i = 0; i < p_boot_record->fat_copies; i++) {
		free(fat_item[i]);
	}
	
	if(clusters) { //v pøípadì že jde o kontrolu konzistence, neni tøeba uvolòovat clustery protože se nenahrály
		for(i = 0; i < p_boot_record->cluster_count; i++) {
			free(clusters[i]);
		}
	}
	
	free(fat_item);
	free(p_boot_record);
	free(new_fat);
	free(clusters);
}

/**
 * Funkce pro otevøení souboru zadaného parametrem path
 * Soubor je uložen v promìnné p_file
 * parametry:	path - otevíraný soubor
 */
int open_file(char *path) {
	p_file = fopen(path, "r+");
	
	//nepodarilo se otevrit soubor
	if(!p_file) return 0;
	
    fseek(p_file, 0, SEEK_SET);
    
    return 1;
}

/**
 * Funkce pro vytvoøení hlavièky souboru boot_record
 * parametry:	p_file - zdrojový soubor
 */
boot_record *create_boot_record(FILE *p_file) {
	boot_record *retval = (boot_record *) malloc(sizeof(boot_record));
	
	fread(retval, sizeof(boot_record), 1, p_file);
	
	return retval;
}

/**
 * Funkce pro vytvoøení záznamu souboru root_directory nachazejícího se uvnitø p_file
 * parametry:	p_file - zdrojový soubor
 */
root_directory *create_root_dir(FILE *p_file) {
	root_directory *retval = (root_directory *) malloc(sizeof(root_directory));
	
	fread(retval, sizeof(root_directory), 1, p_file);
	
	return retval;
}

/**
 * Funkce pro vytvoøení clusteru obsahujícího data souboru
 * parametry:	p_file - zdrojový soubor
 */
char *create_cluster(FILE *p_file) {
	char *retval = (char *)malloc(sizeof(char) * p_boot_record->cluster_size);
	
	fread(retval, sizeof(char) * p_boot_record->cluster_size, 1, p_file);
	
	return retval;
}

/**
 * Funkce pro vytvoøení FAT tabulky souboru
 * parametry:	p_file - zdrojový soubor
 */
unsigned int *create_fat(FILE *p_file) {
	unsigned int *retval = (unsigned int *) malloc (sizeof(unsigned int)*p_boot_record->cluster_count);
	
	fread(retval, sizeof(unsigned int), p_boot_record->cluster_count, p_file);
	
	return retval;
}

/**
 * Funkce pro prohození 2 záznamù v tabulce FAT. Nejdøíve prohodí ukazatele point1 a point2.
 * V pøípadì že je jeden z nich -1 (nic na cluster neukzuje) se prohození pointù neprovede.
 * Potom se provede prohození samotných FAT záznamù požadovaných clusterù.
 * parametry:	point1 - index ukazatele na cluster1
 *				point2 - index ukazatele na cluster2
 *				cluster1 - index FAT záznamu cluster1
 *				cluster2 - index FAT záznamu cluster2
 */
void swap_fat(int point1, int point2, int cluster1, int cluster2) {
	unsigned int tmp_fat;
	if(point1 >= 0) { new_fat[point1] = cluster2; }
	if(point2 >= 0) { new_fat[point2] = cluster1; }
	
	tmp_fat = new_fat[cluster1];
	new_fat[cluster1] = new_fat[cluster2];
	new_fat[cluster2] = tmp_fat;
}

/**
 * Funkce prohození dat v clusterù.
 * parametry:	cluster1 - index cluster1
 *				cluster2 - index cluster2
 */
void swap_clusters(int cluster1, int cluster2) {
	char *tmp_cluster = malloc(sizeof(char) * p_boot_record->cluster_size);
   	strcpy(tmp_cluster, clusters[cluster1]);
  	strcpy(clusters[cluster1], clusters[cluster2]);
   	strcpy(clusters[cluster2], tmp_cluster);
	free(tmp_cluster);
}

/**
 * Funkce pro nalezení clusteru, který ukazuje na zadaný cluster.
 * V pøípadì kdy takový cluster neexistuje vrátí -1.
 * parametry:	position - cluster, pro který se hledá jeho rodiè
 */
int find_cluster_parent(int position) {
	if(new_fat[position] == FAT_UNUSED || new_fat[position] == FAT_BAD_CLUSTER) return -1;
	int i;
	for(i = 0; i < p_boot_record->cluster_count; i++) {
		if(new_fat[i] == position) return i;
	}
	return -1;
}

/**
 * Funkce pro vypoèítání správné pozice prvního clusteru.
 * Poèítá správnou pozici pomocí záznamu velikosti souboru v root_directory
 * parametry:	file_index - index souboru
 *				offset - offset clusterù uvnitø souboru (nemusí vždy hledat 1. cluster)
 */
int correct_first_cluster(int file_index, int offset) {
	int retval = 0, i = 0;
	//projde všechny soubory dokud nenajde hleddaný a poèítá pøi tom jejich celkovou délku
	while(i != file_index) {
		int file_clusters = p_root_directory[i]->file_size/(p_boot_record->cluster_size-1);
		//v pøípadì je soubor plnì zaplnìn je potøeba pøièíst 1 cluster (kvùli dìlení v pøedchozím kroku)s
		if(p_root_directory[i]->file_size%(p_boot_record->cluster_size-1) != 0) file_clusters++;
		retval += file_clusters;
		i++;
	}
	return retval+offset;
}

/**
 * Pomocná funkce pro výpis clusterù souboru. V release verzi nepoužitá.
 */
void print_clusters() {
	int i;
	printf("Vypisuji . . .\n");
    for (i = 0; i < p_boot_record->cluster_count; i++) {    
      	/*if(clusters[i][0] != '\0')
       		printf("Cluster %d - %s\n",i, clusters[i]);*/
    	if(new_fat[i] != FAT_UNUSED) {
	    	if(new_fat[i] == FAT_FILE_END)
	           	printf("%d - FILE_END\n", i);
	       	else if(new_fat[i] == FAT_BAD_CLUSTER)
	          	printf("%d - BAD_CLUSTER\n", i);
	    	else
	          	printf("%d - %d\n", i, new_fat[i]);
        }
    }
    
    /*unsigned int curr;
    for(i = 0; i < p_boot_record->root_directory_max_entries_count; i++) {
    	printf("FILE %d: ", i);
    	curr = p_root_directory[i]->first_cluster;
    	while(curr != FAT_FILE_END) {
    		printf("%s", clusters[curr]);
    		curr = new_fat[curr];
    	}
    	printf("\n");
    }*/
}

/**
 * Funkce pro zápis výsledné podoby FAT do souboru p_file.
 */
void write_result() {
    int i;
    fseek(p_file, 0, SEEK_SET);
    fwrite(p_boot_record, sizeof(boot_record), 1, p_file);
    for(i = 0; i < p_boot_record->fat_copies; i++) {
        fwrite(new_fat, sizeof(unsigned int)*p_boot_record->cluster_count, 1, p_file);
    }
    for(i = 0; i < p_boot_record->root_directory_max_entries_count; i++) {
        fwrite(p_root_directory[i], sizeof(root_directory), 1, p_file);
    }
    for(i = 0; i < p_boot_record->cluster_count; i++) {
        fwrite(clusters[i], sizeof(char) * p_boot_record->cluster_size, 1, p_file);
    }
}

/**
 * Funkce pro naètení všech clusterù souboru do pamìti.
 */
void load_clusters() {
    int i;
    clusters = malloc(sizeof(char *) * p_boot_record->cluster_count);
    for(i = 0; i < p_boot_record->cluster_count; i++) {
    	clusters[i] = create_cluster(p_file);
    }
}

/**
 * Funkce pro naètení boot_record, FAT tabulek a root_directory záznamù.
 */
void load_file() {
    int i;                    
    p_boot_record = create_boot_record(p_file);
       
    unsigned int tmp;
    fat_item = malloc(sizeof(unsigned int *) * p_boot_record->fat_copies);
    for(i = 0; i < p_boot_record->fat_copies; i++) {
    	fat_item[i] = create_fat(p_file);
    }
    
    new_fat = (unsigned int *) malloc (sizeof(unsigned int)*p_boot_record->cluster_count);
    for(i = 0; i < p_boot_record->cluster_count; i++) {
    	new_fat[i] = fat_item[0][i];
    }
    
    p_root_directory = malloc(sizeof(root_directory)*p_boot_record->root_directory_max_entries_count);
  	
    for (i = 0; i < p_boot_record->root_directory_max_entries_count; i++)
    {
		p_root_directory[i] = create_root_dir(p_file);
    }
}
