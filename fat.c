#include "fat.h"
#include "ds.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

// Block numbers for special regions on disk
#define SUPER 0   // Superblock is at block 0
#define TABLE 2   // FAT table starts at block 2
#define DIR 1     // Directory is at block 1

#define SIZE 1024 // General size constant (not used in this stub)

// Superblock structure and magic number for file system identification
#define MAGIC_N           0xAC0010DE
typedef struct{
	int magic;              // Magic number to identify the file system
	int number_blocks;      // Total number of blocks in the file system
	int n_fat_blocks;       // Number of blocks used by the FAT table
	char empty[BLOCK_SIZE-3*sizeof(int)]; // Padding to fill the block
} super;

super sb; // Global superblock variable

// Directory item structure and constants
#define MAX_LETTERS 6      // Maximum file name length
#define OK 1
#define NON_OK 0
typedef struct{
	unsigned char used;         // 1 if entry is used, 0 if free
	char name[MAX_LETTERS+1];   // File name (null-terminated)
	unsigned int length;        // File length in bytes
	unsigned int first;         // First block of the file in FAT
} dir_item;

#define N_ITEMS (BLOCK_SIZE / sizeof(dir_item)) // Number of directory entries per block
dir_item dir[N_ITEMS]; // Directory table in memory

// FAT table constants and pointer
#define FREE 0   // Block is free
#define EOFF 1   // End of file chain
#define BUSY 2   // Block is in use (not standard FAT, but used here)
unsigned int *fat; // Pointer to FAT table in memory

int mountState = 0; // 1 if file system is mounted, 0 otherwise

// Formats the file system (stub)
int fat_format(){ 
	if(mountState){//sistema ta montado, nao pode formatar
		errno = EBUSY; 
		return -1;
	}

	//inicializa o superbloco, sera q precisa ser variavel auxiliar?
	/*super aux_sb;
	aux_sb.magic = MAGIC_N;
	aux_sb.number_blocks = ds_size();
	aux_sb.n_fat_blocks = (int)ceil((float)aux_sb.number_blocks * sizeof(unsigned int) / BLOCK_SIZE);

	//escreve o superbloco no disco
	ds_write(SUPER, (char *)&aux_sb);

	//inicializa o diretorio, macando as entradas como nao usadas
	for(int i = 0; i < N_ITEMS; i++){
		dir[i].used = 0;
	}
	ds_write(DIR, (char *)dir);

	//inicializa a fat
	fat = malloc(sizeof(unsigned int) * aux_sb.number_blocks);
	if(!fat){
		errno = ENOMEM;
		return -1;
	}
	for(int i = 0; i < aux_sb.number_blocks; i++){
		fat[i] = FREE;
	}

	//grava fat nos blocos correspondentes
	for(int i = 0; i < aux_sb.n_fat_blocks; i ++){
		ds_write(TABLE + i, (char *)(fat + 1 * BLOCK_SIZE / sizeof(unsigned int)));
	}

	free(fat);
  	return 0;*/

	sb.magic = MAGIC_N;
	sb.number_blocks = ds_size();
	sb.n_fat_blocks = (int)ceil((float)sb.number_blocks * sizeof(unsigned int) / BLOCK_SIZE);

	//escreve o superbloco no disco
	ds_write(SUPER, (char *)&sb);

	//inicializa o diretorio, macando as entradas como nao usadas
	for(int i = 0; i < N_ITEMS; i++){
		dir[i].used = 0;
	}
	ds_write(DIR, (char *)dir);

	//inicializa a fat
	fat = malloc(sizeof(unsigned int) * sb.number_blocks);
	if(!fat){
		errno = ENOMEM;
		return -1;
	}
	for(int i = 0; i < sb.number_blocks; i++){
		fat[i] = FREE;
	}

	//grava fat nos blocos correspondentes
	for(int i = 0; i < sb.n_fat_blocks; i ++){
		ds_write(TABLE + i, (char *)(fat + i * BLOCK_SIZE / sizeof(unsigned int)));
	}
	/*char fat_buffer[BLOCK_SIZE];//assim funciona tambem
	int entries_per_block = BLOCK_SIZE / sizeof(unsigned int);

	for (int i = 0; i < sb.n_fat_blocks; i++) {
		memcpy(fat_buffer, fat + i * entries_per_block, BLOCK_SIZE);
		ds_write(TABLE + i, fat_buffer);
	}*/


	free(fat);
  	return 0;
}

// Prints debugging information about the file system (stub)
void fat_debug(){
	// superblock info
	super aux_sb;
	//why aux? we could possibly mess up the other operations if the global superblock variable was modified
	ds_read(SUPER, (char*) &aux_sb);
	printf("superblock:\n");
	if (aux_sb.magic == MAGIC_N) {
		printf("\tmagic is ok\n");
		printf("\t%d blocks\n", aux_sb.number_blocks);
		printf("\t%d block fat\n", aux_sb.n_fat_blocks);
	} else {
		printf("\tmagic is NOT ok\n");
	}

	//read fat. we must be able to debug the filesystem even if it is not mounted
	unsigned int* aux_fat = malloc(sizeof(unsigned int) * aux_sb.number_blocks);
	for (int i = 0; i < aux_sb.n_fat_blocks; i++) {
		// read every row
		ds_read(TABLE + i, (char*) (aux_fat + i * BLOCK_SIZE / sizeof(unsigned int)));
	}

	// read directory
	dir_item aux_dir[N_ITEMS]; 
	ds_read(DIR, (char*) aux_dir);

	unsigned int block;
	// loop through directory
	for (int i = 0; i < N_ITEMS; i++) {
		if (aux_dir[i].used) {
			printf("File \"%s\":\n", aux_dir[i].name);
			printf("\tsize: %u bytes\n", aux_dir[i].length);

			printf("\tBlocks:");
			block = aux_dir[i].first;
			while (block != EOFF && block < aux_sb.number_blocks) {
				printf(" %u", block);
				block = aux_fat[block];
			}
			printf("\n");
		}
	}

	free(aux_fat);
}

// Mounts the file system (stub)
int fat_mount(){
	if(mountState == 1){//testa se ja estiver montado, se tiver vai dar falha na montagem
		errno = EBUSY;
		return -1;
	}
  	// read superblock
	ds_read(SUPER, (char*) &sb);
	if (sb.magic == MAGIC_N) {
		// bring FAT to memory
		fat = malloc(sizeof(unsigned int) * sb.number_blocks);
		for (int i = 0; i < sb.n_fat_blocks; i++) {
			ds_read(TABLE + i, (char*) (fat + i * BLOCK_SIZE / sizeof(unsigned int)));
		}

		// bring DIR to memory
		ds_read(DIR, (char*) dir);
		// filesystem mounted successfully
		mountState = 1;
		//free(fat);
		return 0;
	} else {
		errno = EINVAL;
		return 1;
	}
	
}

// Creates a new file in the file system (stub)
int fat_create(char *name){
	//Check if file system is mounted
	if(!mountState) {
		errno = EINVAL;
		return -1;
	}

	// Check for valid name
	if(!name || strlen(name) > MAX_LETTERS) {
		errno = EINVAL;
		return -1;
	}

	// Check if file already exists
	for(int i = 0; i < N_ITEMS; i++) {
		if(dir[i].used && !strcmp(dir[i].name, name)) {
			errno = EEXIST;
			return -1;
		}
	}

	// Find a free directory entry
	int free_index = -1;
	for(int i = 0; i < N_ITEMS; i++) {
		if(!dir[i].used) {
			free_index = i;
			break;
		}
	}
	if(free_index == -1) {
		errno = ENOSPC; // No space left in directory
		return -1;
	}
	// Find a free block in the FAT
	int free_block = -1;
	for(int i = 0; i < sb.number_blocks; i++) {
		if(fat[i] == FREE) {
			free_block = i;
			break;
		}
	}
	if(free_block == -1) {
		errno = ENOSPC; // No space left on disk
		return -1;
	}

	// Mark block as end of file in FAT
	fat[free_block] = EOFF;

	// Fill in the directory entry
	dir[free_index].used = 1; // Mark entry as used
	strncpy(dir[free_index].name, name, MAX_LETTERS); // Copy name
	dir[free_index].name[MAX_LETTERS] = '\0'; // Null-terminate
	dir[free_index].length = 0; // Initialize length to 0
	dir[free_index].first = free_block; // Set first block
	
	// Write directory and FAT back to disk
	ds_write(DIR, (char *)dir); // Write directory to disk
	for (int i = 0; i < sb.n_fat_blocks; i++) {
		ds_write(TABLE + i, (char *)(fat + i * BLOCK_SIZE / sizeof(unsigned int))); // Write FAT blocks to disk
	}
	
	/*char fat_buffer[BLOCK_SIZE];//assim funciona tambem
	int entries_per_block = BLOCK_SIZE / sizeof(unsigned int);

	for (int i = 0; i < sb.n_fat_blocks; i++) {
		memcpy(fat_buffer, fat + i * entries_per_block, BLOCK_SIZE);
		ds_write(TABLE + i, fat_buffer);
	}*/


	
	return 0;
}

// Deletes a file from the file system (stub)
int fat_delete( char *name){
	//Check if file system is mounted
	if(!mountState) {
		errno = EINVAL;
		return -1;
	}

	// Check for valid name
	if(!name || strlen(name) > MAX_LETTERS) {
		errno = EINVAL;
		return -1;
	}

	//procura o arquivo no diretorio
	int arq_encontrado = -1;
	for(int i = 0; i < N_ITEMS; i++) {
		if(dir[i].used && strcmp(dir[i].name, name) == 0) {
			arq_encontrado = i;
			break;
		}
	}
	if(arq_encontrado == -1){
		errno = ENOENT;//arquivo não encontrado
		return -1;
	}

	//libera blocos da fat
	unsigned int aux = dir[arq_encontrado].first;//começa no primeiro bloco do arquivo
	while(aux != EOFF && aux < sb.number_blocks){
		unsigned int prox = fat[aux];//pega o indica do proximo bloco do arquivo guardado no fat
		fat[aux] = FREE;
		aux = prox;//passa para o proximo bloco
	}

	//marca a entrada do diretorio como livre
	dir[arq_encontrado].used = 0;

	//escreve a fat e o diretorio no disco
	//garante que as alteracoes na ram sejam feitas no disco tbm
	for(int i = 0; i < sb.n_fat_blocks; i++){
		ds_write(TABLE + i, (char *)(fat + i * BLOCK_SIZE / sizeof(unsigned int)));
	}
	/*char fat_buffer[BLOCK_SIZE];//assim funciona tambem
	int entries_per_block = BLOCK_SIZE / sizeof(unsigned int);

	for (int i = 0; i < sb.n_fat_blocks; i++) {
		memcpy(fat_buffer, fat + i * entries_per_block, BLOCK_SIZE);
		ds_write(TABLE + i, fat_buffer);
	}*/

	ds_write(DIR, (char *)dir);


  	return 0;
}

// Gets the size of a file in bytes (stub)
int fat_getsize( char *name){ 
	return 0;
}

// Reads data from a file into a buffer (stub)
// Returns the number of bytes read
int fat_read( char *name, char *buff, int length, int offset){
	return 0;
}

// Writes data from a buffer to a file (stub)
// Returns the number of bytes written
int fat_write( char *name, const char *buff, int length, int offset){
	return 0;
}
