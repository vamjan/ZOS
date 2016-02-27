#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include "fat.h"

FILE *p_file;
boot_record *p_boot_record;
root_directory **p_root_directory;	//pole o velikosti root_directory_max_entries_count, jsou v nem ulozeny inforamce o souborech
unsigned int **fat_item, *new_fat;	//pole o velikosti cluster_count, reprezetuje puvodni FAT
char **clusters;

void free_data() {
	int i;
	for(i = 0; i < p_boot_record->root_directory_max_entries_count; i++) {
		free(p_root_directory[i]);
	}
	
	for(i = 0; i < p_boot_record->fat_copies; i++) {
		free(fat_item[i]);
	}
	
	if(clusters) {
		for(i = 0; i < p_boot_record->cluster_count; i++) {
			free(clusters[i]);
		}
	}
	
	free(fat_item);
	free(p_boot_record);
	free(new_fat);
	free(clusters);
}

int open_file(char *path) {
	p_file = fopen(path, "r+");
	
	//nepodarilo se otevrit soubor
	if(!p_file) return 0;
	
    fseek(p_file, 0, SEEK_SET);
    
    return 1;
}

boot_record *create_boot_record(FILE *p_file) {
	boot_record *retval = (boot_record *) malloc(sizeof(boot_record));
	
	fread(retval, sizeof(boot_record), 1, p_file);
	
	return retval;
}

root_directory *create_root_dir(FILE *p_file) {
	root_directory *retval = (root_directory *) malloc(sizeof(root_directory));
	
	fread(retval, sizeof(root_directory), 1, p_file);
	
	return retval;
}

char *create_cluster(FILE *p_file) {
	char *retval = (char *)malloc(sizeof(char) * p_boot_record->cluster_size);
	
	fread(retval, sizeof(char) * p_boot_record->cluster_size, 1, p_file);
	
	return retval;
}

unsigned int *create_fat(FILE *p_file) {
	unsigned int *retval = (unsigned int *) malloc (sizeof(unsigned int)*p_boot_record->cluster_count);
	
	fread(retval, sizeof(unsigned int), p_boot_record->cluster_count, p_file);
	
	return retval;
}

void swap_fat(int point1, int point2, int cluster1, int cluster2) {
	unsigned int tmp_fat;
	if(point1 >= 0) { new_fat[point1] = cluster2; }
	if(point2 >= 0) { new_fat[point2] = cluster1; }
	
	tmp_fat = new_fat[cluster1];
	new_fat[cluster1] = new_fat[cluster2];
	new_fat[cluster2] = tmp_fat;
}

void swap_clusters(int cluster1, int cluster2) {
	char *tmp_cluster = malloc(sizeof(char) * p_boot_record->cluster_size);
   	strcpy(tmp_cluster, clusters[cluster1]);
  	strcpy(clusters[cluster1], clusters[cluster2]);
   	strcpy(clusters[cluster2], tmp_cluster);
	free(tmp_cluster);
}

int find_cluster_parent(int position) {
	if(new_fat[position] == FAT_UNUSED || new_fat[position] == FAT_BAD_CLUSTER) return -1;
	int i;
	for(i = 0; i < p_boot_record->cluster_count; i++) {
		if(new_fat[i] == position) return i;
	}
	return -1;
}

int correct_first_cluster(int file_index, int offset) {
	int retval = 0, i = 0;
	while(i != file_index) {
		int file_clusters = p_root_directory[i]->file_size/(p_boot_record->cluster_size-1);
		if(p_root_directory[i]->file_size%(p_boot_record->cluster_size-1) != 0) file_clusters++;
		retval += file_clusters;
		i++;
	}
	return retval+offset;
}

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

void load_clusters() {
    int i;
    clusters = malloc(sizeof(char *) * p_boot_record->cluster_count);
    for(i = 0; i < p_boot_record->cluster_count; i++) {
    	clusters[i] = create_cluster(p_file);
    }
}

void load_file() {
    int i;                    
    //alokujeme pamet
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
