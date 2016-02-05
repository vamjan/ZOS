#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "fat.h"

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

void load_file(char *path) {
	//otevru soubor a pro jistotu skocim na zacatek
	FILE *p_file;
	p_file = fopen(path, "r");
    fseek(p_file, SEEK_SET, 0);
    
    //pointery na struktury root a boot                         
    boot_record *p_boot_record;
    root_directory **p_root_directory;
    
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
    unsigned int *fat_item, tmp;
    fat_item = (unsigned int *) malloc (sizeof(unsigned int)*p_boot_record->cluster_count);
    int j;
    for(j = 0; j < p_boot_record->fat_copies; j++)
    {
        printf("\nFAT KOPIE %d\n", j + 1);
    	for(l = 0; l < p_boot_record->cluster_count; l++)
      	{
       		fread(&fat_item[l], sizeof(*fat_item), 1, p_file);
          	if(fat_item[l] != FAT_UNUSED)
          	{
            	if(fat_item[l] == FAT_FILE_END)
                	printf("%d - FILE_END\n", l);
            	else if(fat_item[l] == FAT_BAD_CLUSTER)
                	printf("%d - BAD_CLUSTER\n", l);
            	else
              		printf("%d - %d\n", l, fat_item[l]);
    		}
		}
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
    
    int position, data_start = ftell(p_file);
    unsigned int curr;
    char *p_cluster = malloc(sizeof(char) * p_boot_record->cluster_size);
    
    for(i = 0; i < p_boot_record->root_directory_max_entries_count; i++) {
    	curr = p_root_directory[i]->first_cluster;
    	while(curr != FAT_FILE_END) {
    		fseek(p_file, SEEK_SET, data_start + curr*p_boot_record->cluster_size);
    		fread(p_cluster, sizeof(char) * p_boot_record->cluster_size, 1, p_file);
    		printf("%s", p_cluster);
    		curr = fat_item[curr];
    	}
    	printf("\n");
    }
}
