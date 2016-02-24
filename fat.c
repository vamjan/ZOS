#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "fat.h"

FILE *p_file;
boot_record *p_boot_record;

root_directory **p_root_directory;	//pole o velikosti root_directory_max_entries_count, jsou v nem ulozeny inforamce o souborech
unsigned int **fat_item, *new_fat;	//pole o velikosti cluster_count, reprezetuje puvodni FAT
char **clusters;

void free_all() {
	//todo
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
	char *retval = (char *)malloc(sizeof(char) * p_boot_record->cluster_count);
	
	fread(retval, sizeof(char) * p_boot_record->cluster_size, 1, p_file);
	
	return retval;
}

unsigned int *create_fat(FILE *p_file) {
	unsigned int *retval = (unsigned int *) malloc (sizeof(unsigned int)*p_boot_record->cluster_count);
	
	fread(retval, sizeof(unsigned int), p_boot_record->cluster_count, p_file);
	
	return retval;
}

void swap_clusters(int point1, int point2, int cluster1, int cluster2) {
	if(cluster1 > p_boot_record->cluster_count || cluster2 > p_boot_record->cluster_count) return;
	printf("point1 = %d, point2 = %d, cluster1 = %d, cluster2 = %d\n", point1, point2, cluster1, cluster2);
	unsigned int tmp_fat;
    if(cluster1 == cluster2) { return; }
	if(point1 >= 0) { new_fat[point1] = cluster2; }
	if(point2 >= 0) { new_fat[point2] = cluster1; }
	
	tmp_fat = new_fat[cluster1];
	new_fat[cluster1] = new_fat[cluster2];
	new_fat[cluster2] = tmp_fat;
	
	char *tmp_cluster = malloc(sizeof(char) * p_boot_record->cluster_size);
   	strcpy(tmp_cluster, clusters[cluster1]);
  	strcpy(clusters[cluster1], clusters[cluster2]);
   	strcpy(clusters[cluster2], tmp_cluster);
    
	free(tmp_cluster);
}

int find_cluster_parent(int position) {
	if(new_fat[position] == FAT_UNUSED || new_fat[position] == FAT_BAD_CLUSTER) return p_boot_record->cluster_count+1;
	int i;
	for(i = 0; i < p_boot_record->cluster_count; i++) {
		if(new_fat[i] == position) return i;
	}
	return -1;
}

int correct_first_cluster(int file_index) {
	int retval = 0, i = 0;
	while(i != file_index) {
		int file_clusters = p_root_directory[i]->file_size/(p_boot_record->cluster_size-1);
		if(p_root_directory[i]->file_size%(p_boot_record->cluster_size-1) != 0) file_clusters++;
		retval += file_clusters;
		i++;
	}
	return retval;
}

void defrag() {
	int i, j;
	for(i = 0; i < p_boot_record->root_directory_max_entries_count; i++) {
		int first_cluster = correct_first_cluster(i);
		int file_clusters = p_root_directory[i]->file_size/(p_boot_record->cluster_size);
		if(p_root_directory[i]->file_size%(p_boot_record->cluster_size) != 0) file_clusters++;
		printf("first cluster of file %d = %d\n", i, first_cluster);
		
		printf("cluster %d of file %d is being swaped\n", 1, i);//"\ttext - %s\n", 1, i, clusters[p_root_directory[i]->first_cluster]);
		swap_clusters(-1, find_cluster_parent(p_root_directory[i]->first_cluster),first_cluster, p_root_directory[i]->first_cluster);
		p_root_directory[i]->first_cluster = first_cluster;
		
		printf("file clusters = %d\n", file_clusters);
		
		for(j = 0; j < file_clusters-1; j++) {
			printf("cluster %d of file %d is being swaped\n", j+2, i);//"\ttext - %s\n", j+2, i, clusters[new_fat[first_cluster+j]]);
			swap_clusters(find_cluster_parent(first_cluster+j+1), first_cluster+j, first_cluster+j+1, new_fat[first_cluster+j]);
		}
		
		printf("\n");
	}
	
	int l;
	for(l = 0; l < p_boot_record->cluster_count/20; l++) {
	    if(new_fat[l] == FAT_UNUSED)
	        printf("%d - FAT_UNUSED\t", l);
	    else if(new_fat[l] == FAT_FILE_END)
	        printf("%d - FILE_END\n", l);
	    else if(new_fat[l] == FAT_BAD_CLUSTER)
	        printf("%d - BAD_CLUSTER\t", l);
	    else
	        printf("%d - %d\t", l, new_fat[l]);
	}
	printf("\n");
}

void print_clusters() {
	int i;
	printf("Vypisuji . . .\n");
    for (i = 0; i < p_boot_record->cluster_count; i++) {    
      	if(clusters[i][0] != '\0')
       		printf("Cluster %d - %s\n",i, clusters[i]);
    }
    
    unsigned int curr;
    for(i = 0; i < p_boot_record->root_directory_max_entries_count; i++) {
    	curr = p_root_directory[i]->first_cluster;
    	while(curr != FAT_FILE_END) {
    		printf("%s", clusters[curr]);
    		curr = new_fat[curr];
    	}
    	printf("\n");
    }
}

void write_stuff() {
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

void load_file() {
                        
    //alokujeme pamet
    p_boot_record = create_boot_record(p_file);
    
    //prectu boot
    printf("-------------------------------------------------------- \n");
    printf("BOOT RECORD \n");
    printf("-------------------------------------------------------- \n");
    printf("volume_descriptor :%s\n",p_boot_record->volume_descriptor);
  	printf("fat_type :%d\n",p_boot_record->fat_type);
  	printf("fat_copies :%d\n",p_boot_record->fat_copies);
  	printf("cluster_size :%d\n",p_boot_record->cluster_size);
  	printf("root_directory_max_entries_count :%ld\n",p_boot_record->root_directory_max_entries_count);    
  	printf("cluster count :%d\n",p_boot_record->cluster_count);
  	printf("reserved clusters :%d\n",p_boot_record->reserved_cluster_count);
  	printf("signature :%s\n",p_boot_record->signature);
    
    //prectu fat_copies krat 
    printf("-------------------------------------------------------- \n");
    printf("FAT \n");
    printf("-------------------------------------------------------- \n");
    
    long l;
    unsigned int tmp;
    fat_item = malloc(sizeof(unsigned int *) * p_boot_record->fat_copies);
    for(l = 0; l < p_boot_record->fat_copies; l++) {
    	fat_item[l] = create_fat(p_file);
    }
    int j;
    for(j = 0; j < p_boot_record->fat_copies; j++) {
        printf("\nFAT KOPIE %d\n", j + 1);
    	for(l = 0; l < p_boot_record->cluster_count; l++) {
            if(l < p_boot_record->cluster_count/20) {
	          	if(fat_item[j][l] == FAT_UNUSED)
	                printf("%d - FAT_UNUSED\n", l);
	           	else if(fat_item[j][l] == FAT_FILE_END)
	               	printf("%d - FILE_END\n", l);
	           	else if(fat_item[j][l] == FAT_BAD_CLUSTER)
	              	printf("%d - BAD_CLUSTER\n", l);
	           	else
	          		printf("%d - %d\n", l, fat_item[j][l]);
            }
		}
    }
    
    new_fat = (unsigned int *) malloc (sizeof(unsigned int)*p_boot_record->cluster_count);
    for(l = 0; l < p_boot_record->cluster_count; l++) {
    	new_fat[l] = fat_item[0][l];
    }
    
    //prectu root tolikrat polik je maximalni pocet zaznamu v bootu - root_directory_max_entries_count        
    printf("-------------------------------------------------------- \n");
    printf("ROOT DIRECTORY \n");
    printf("-------------------------------------------------------- \n");
    
    p_root_directory = malloc(sizeof(root_directory)*p_boot_record->root_directory_max_entries_count);
  	int i;
    for (i = 0; i < p_boot_record->root_directory_max_entries_count; i++)
    {
		p_root_directory[i] = create_root_dir(p_file);
	    printf("FILE %d \n",i);
	    printf("file_name :%s\n",p_root_directory[i]->file_name); 
	    printf("file_mod :%s\n",p_root_directory[i]->file_mod);
	    printf("file_type :%d\n",p_root_directory[i]->file_type);
	    printf("file_size :%d\n",p_root_directory[i]->file_size);
	    printf("first_cluster :%d\n",p_root_directory[i]->first_cluster); 
    }
    
    printf("-------------------------------------------------------- \n");
    printf("CLUSTERY - OBSAH \n");
    printf("-------------------------------------------------------- \n");
    
    clusters = malloc(sizeof(char *) * p_boot_record->cluster_count);
    for(i = 0; i < p_boot_record->cluster_count; i++) {
    	clusters[i] = create_cluster(p_file);
    }

}
