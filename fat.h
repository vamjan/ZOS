#ifndef _FAT_H
#define _FAT_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

//definice na vyznam hodnot FAT tabulky
#define FAT_UNUSED 65535
#define FAT_FILE_END 65534
#define FAT_BAD_CLUSTER 65533

//struktura na boot record - nova verze
typedef struct {
    char volume_descriptor[251];              //popis
    int fat_type;                             //typ FAT - pocet clusteru = 2^fat_type (priklad FAT 12 = 4096)
    int fat_copies;                           //kolikrat je za sebou v souboru ulozena FAT
    unsigned int cluster_size;                //velikost clusteru ve znacich (n x char) + '/0' - tedy 128 znamena 127 vyznamovych znaku + '/0'
    long root_directory_max_entries_count;    //pocet polozek v root_directory = pocet souboru MAXIMALNE, nikoliv aktualne - pro urceni kde zacinaji data - resp velikost root_directory v souboru
    unsigned int cluster_count;               //pocet pouzitelnych clusteru (2^fat_type - reserved_cluster_count)
    unsigned int reserved_cluster_count;      //pocet rezervovanych clusteru pro systemove polozky
    char signature[4];                        //pro vstupni data od vyucujicich konzistence FAT - "OK","NOK","FAI" -FAT2 != FATx / FAIL - ve FAT1 == FAT2 == FAT3, ale obsahuje chyby, nutna kontrola v poradku / FAT1 != 
} boot_record;

//struktura na root directory - nova verze
typedef struct{
    char file_name[13];             //8+3 format + '/0'
    char file_mod[10];              //unix atributy souboru+ '/0'
    short file_type;                //0 = soubor, 1 = adresar
    long file_size;                 //pocet znaku v souboru 
    unsigned int first_cluster;     //cluster ve FAT, kde soubor zacina - POZOR v cislovani root_directory ma prvni cluster index 0 (viz soubor a.txt)
} root_directory;

extern boot_record *p_boot_record;
extern root_directory **p_root_directory;
extern unsigned int **fat_item, *new_fat;
extern char **clusters;

//funkce pro praci s FAT
void free_data();
int open_file(char *path);
boot_record *create_boot_record(FILE *p_file);
root_directory *create_root_dir(FILE *p_file);
char *create_cluster(FILE *p_file);
unsigned int *create_fat(FILE *p_file);

void swap_fat(int point1, int point2, int cluster1, int cluster2);
void swap_clusters(int cluster1, int cluster2);
int find_cluster_parent(int position);
int correct_first_cluster(int file_index, int offset);

void print_clusters();
void write_result();
void load_clusters();
void load_file();

#endif
