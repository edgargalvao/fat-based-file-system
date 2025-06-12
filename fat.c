#include "fat.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "ds.h"

// Block numbers for special regions on disk
#define SUPER 0  // Superblock is at block 0
#define TABLE 2  // FAT table starts at block 2
#define DIR 1    // Directory is at block 1

#define SIZE 1024  // General size constant (not used in this stub)

// Superblock structure and magic number for file system identification
#define MAGIC_N 0xAC0010DE
typedef struct {
  int magic;          // Magic number to identify the file system
  int number_blocks;  // Total number of blocks in the file system
  int n_fat_blocks;   // Number of blocks used by the FAT table
  char empty[BLOCK_SIZE - 3 * sizeof(int)];  // Padding to fill the block
} super;

super sb;  // Global superblock variable

// Directory item structure and constants
#define MAX_LETTERS 6  // Maximum file name length
#define OK 1
#define NON_OK 0
typedef struct {
  unsigned char used;          // 1 if entry is used, 0 if free
  char name[MAX_LETTERS + 1];  // File name (null-terminated)
  unsigned int length;         // File length in bytes
  unsigned int first;          // First block of the file in FAT
} dir_item;

#define N_ITEMS \
  (BLOCK_SIZE / sizeof(dir_item))  // Number of directory entries per block
dir_item dir[N_ITEMS];             // Directory table in memory

// FAT table constants and pointer
#define FREE 0      // Block is free
#define EOFF 1      // End of file chain
#define BUSY 2      // Block is in use (not standard FAT, but used here)
unsigned int fat[BLOCK_SIZE * 4096 / sizeof(unsigned int)];  // Global FAT table (assuming max 4096 blocks)

int mountState = 0;  // 1 if file system is mounted, 0 otherwise

// Formats the file system
int fat_format() {
  if (mountState) {  // sistema ta montado, nao pode formatar
    errno = EBUSY;
    return -1;
  }

  sb.magic = MAGIC_N;
  sb.number_blocks = ds_size();
  sb.n_fat_blocks =
      (int)ceil((float)sb.number_blocks * sizeof(unsigned int) / BLOCK_SIZE);

  // escreve o superbloco no disco
  ds_write(SUPER, (char *)&sb);

  // inicializa o diretorio, macando as entradas como nao usadas
  for (int i = 0; i < N_ITEMS; i++) {
    dir[i].used = 0;
  }
  ds_write(DIR, (char *)dir);

  // inicializa a fat
  for (int i = 0; i < sb.number_blocks; i++) {
    fat[i] = FREE;
  }

  // Marcar blocos reservados como ocupados
  fat[SUPER] = BUSY;
  fat[DIR] = BUSY;
  for (int i = 0; i < sb.n_fat_blocks; i++) {
    fat[TABLE + i] = BUSY;
  }

  char fat_buffer[BLOCK_SIZE];
  int entries_per_block = BLOCK_SIZE / sizeof(unsigned int);

  // copiar buffer pra FAT
  for (int i = 0; i < sb.n_fat_blocks; i++) {
    memcpy(fat_buffer, fat + i * entries_per_block, BLOCK_SIZE);
    ds_write(TABLE + i, fat_buffer);
  }

  return 0;
}

// Prints debugging information about the file system
void fat_debug() {
  // superblock info
  super aux_sb;
  ds_read(SUPER, (char *)&aux_sb);
  printf("superblock:\n");
  if (aux_sb.magic == MAGIC_N) {
    printf("\tmagic is ok\n");
    printf("\t%d blocks\n", aux_sb.number_blocks);
    printf("\t%d block fat\n", aux_sb.n_fat_blocks);
  } else {
    printf("\tmagic is NOT ok\n");
  }

  // read fat from disk to global fat array
  for (int i = 0; i < aux_sb.n_fat_blocks; i++) {
    ds_read(TABLE + i, (char *)(fat + i * BLOCK_SIZE / sizeof(unsigned int)));
  }

  // read directory from disk to global dir array
  ds_read(DIR, (char *)dir);

  unsigned int block;
  for (int i = 0; i < N_ITEMS; i++) {
    if (dir[i].used) {
      printf("File \"%s\":\n", dir[i].name);
      printf("\tsize: %u bytes\n", dir[i].length);

      printf("\tBlocks:");
      block = dir[i].first;
      int safety_counter = 0;

      while (block != EOFF && block < aux_sb.number_blocks &&
             safety_counter < aux_sb.number_blocks) {
        printf("%u ", block);
        block = fat[block];
        safety_counter++;
      }
      printf("\n");
    }
  }
}

// Mounts the file system
int fat_mount() {
  if (mountState == 1) {
    errno = EBUSY;
    return -1;
  }
  
  // read superblock
  ds_read(SUPER, (char *)&sb);
  if (sb.magic == MAGIC_N) {
    // bring FAT to memory (global fat array)
    for (int i = 0; i < sb.n_fat_blocks; i++) {
      ds_read(TABLE + i, (char *)(fat + i * BLOCK_SIZE / sizeof(unsigned int)));
    }

    // bring DIR to memory (global dir array)
    ds_read(DIR, (char *)dir);
    
    // filesystem mounted successfully
    mountState = 1;
    return 0;
  } else {
    errno = EINVAL;
    return -1;
  }
}

// Creates a new file in the file system
int fat_create(char *name) {
  // Check if file system is mounted
  if (!mountState) {
    errno = EINVAL;
    return -1;
  }

  // Check for valid name
  if (!name || strlen(name) > MAX_LETTERS) {
    errno = EINVAL;
    return -1;
  }

  // Check if file already exists
  for (int i = 0; i < N_ITEMS; i++) {
    if (dir[i].used && !strcmp(dir[i].name, name)) {
      errno = EEXIST;
      return -1;
    }
  }

  // Find a free directory entry
  int free_index = -1;
  for (int i = 0; i < N_ITEMS; i++) {
    if (!dir[i].used) {
      free_index = i;
      break;
    }
  }
  if (free_index == -1) {
    errno = ENOSPC;
    return -1;
  }

  // Find a free block in the FAT
  int free_block = -1;
  for (int i = TABLE + sb.n_fat_blocks; i < sb.number_blocks; i++) {
    if (fat[i] == FREE) {
      free_block = i;
      break;
    }
  }
  if (free_block == -1) {
    errno = ENOSPC;
    return -1;
  }

  // Mark block as end of file in FAT
  fat[free_block] = EOFF;

  // Fill in the directory entry
  dir[free_index].used = 1;
  strncpy(dir[free_index].name, name, MAX_LETTERS);
  dir[free_index].name[MAX_LETTERS] = '\0';
  dir[free_index].length = 0;
  dir[free_index].first = free_block;

  // Write directory and FAT back to disk
  ds_write(DIR, (char *)dir);
  for (int i = 0; i < sb.n_fat_blocks; i++) {
    ds_write(TABLE + i, (char *)(fat + i * BLOCK_SIZE / sizeof(unsigned int)));
  }

  return 0;
}

// Deletes a file from the file system
int fat_delete(char *name) {
  if (!mountState) {
    errno = EINVAL;
    return -1;
  }

  if (!name || strlen(name) > MAX_LETTERS) {
    errno = EINVAL;
    return -1;
  }

  // procura o arquivo no diretorio
  int arq_encontrado = -1;
  for (int i = 0; i < N_ITEMS; i++) {
    if (dir[i].used && strcmp(dir[i].name, name) == 0) {
      arq_encontrado = i;
      break;
    }
  }
  if (arq_encontrado == -1) {
    errno = ENOENT;
    return -1;
  }

  // libera blocos da fat
  unsigned int aux = dir[arq_encontrado].first;
  while (aux != EOFF && aux < sb.number_blocks) {
    unsigned int prox = fat[aux];
    fat[aux] = FREE;
    aux = prox;
  }

  // marca a entrada do diretorio como livre
  dir[arq_encontrado].used = 0;

  // escreve a fat e o diretorio no disco
  for (int i = 0; i < sb.n_fat_blocks; i++) {
    ds_write(TABLE + i, (char *)(fat + i * BLOCK_SIZE / sizeof(unsigned int)));
  }

  ds_write(DIR, (char *)dir);

  return 0;
}

// Gets the size of a file in bytes
int fat_getsize(char *name) {
  if (!name || strlen(name) > MAX_LETTERS) {
    errno = EINVAL;
    return -1;
  }

  if (!mountState) {
    errno = EINVAL;
    return -1;
  }

  // procura o arquivo no diretorio
  int arq_encontrado = -1;
  for (int i = 0; i < N_ITEMS; i++) {
    if (dir[i].used && strcmp(dir[i].name, name) == 0) {
      arq_encontrado = i;
      break;
    }
  }
  if (arq_encontrado == -1) {
    errno = ENOENT;
    return -1;
  }
  return dir[arq_encontrado].length;
}

// Reads data from a file into a buffer
int fat_read(char *name, char *buff, int length, int offset) {
  if (!mountState) {
    errno = EINVAL;
    return -1;
  }

  if (!name || strlen(name) > MAX_LETTERS) {
    errno = EINVAL;
    return -1;
  }

  // procura o arquivo no diretorio
  int arq_encontrado = -1;
  for (int i = 0; i < N_ITEMS; i++) {
    if (dir[i].used && strcmp(dir[i].name, name) == 0) {
      arq_encontrado = i;
      break;
    }
  }
  if (arq_encontrado == -1) {
    errno = ENOENT;
    return -1;
  }

  if (offset >= dir[arq_encontrado].length) {
    errno = EINVAL;
    return -1;
  }

  int readable;
  if (offset + length > dir[arq_encontrado].length) {
    readable = dir[arq_encontrado].length - offset;
  } else {
    readable = length;
  }

  unsigned int current = dir[arq_encontrado].first;
  int skip_blocks = offset / BLOCK_SIZE;
  int block_offset = offset % BLOCK_SIZE;

  // ir para o offset
  for (int i = 0; i < skip_blocks; i++) {
    if (current == EOFF || current >= sb.number_blocks) {
      return 0;
    }
    current = fat[current];
  }

  int bytes_read = 0;
  char temp_block[BLOCK_SIZE];

  // ler os blocos
  while (bytes_read < readable && current != EOFF && current < sb.number_blocks) {
    ds_read(current, temp_block);

    int start = (bytes_read == 0) ? block_offset : 0;
    int block_remaining = BLOCK_SIZE - start;

    int bytes_to_copy = (readable - bytes_read < block_remaining) 
                        ? readable - bytes_read 
                        : block_remaining;

    memcpy(buff + bytes_read, temp_block + start, bytes_to_copy);
    bytes_read += bytes_to_copy;

    current = fat[current];
  }
  return bytes_read;
}

// Writes data from a buffer to a file
int fat_write(char *name, const char *buff, int length, int offset) {
  if (!mountState || !name || strlen(name) > MAX_LETTERS || offset < 0) {
    errno = EINVAL;
    return -1;
  }

  // Procurar o arquivo no diretório
  int arq_encontrado = -1;
  for (int i = 0; i < N_ITEMS; i++) {
    if (dir[i].used && strcmp(dir[i].name, name) == 0) {
      arq_encontrado = i;
      break;
    }
  }

  if (arq_encontrado == -1) {
    errno = ENOENT;
    return -1;
  }

  int data_start = TABLE + sb.n_fat_blocks;
  int writable = length;

  // Verificar se há blocos suficientes disponíveis
  unsigned int current = dir[arq_encontrado].first;
  int total_needed = (offset + length + BLOCK_SIZE - 1) / BLOCK_SIZE;
  int already_allocated = 0;

  unsigned int temp = current;
  while (temp != EOFF) {
    if (temp >= sb.number_blocks) {
      errno = EINVAL;
      return -1;
    }
    already_allocated++;
    temp = fat[temp];
  }

  int new_blocks_needed = total_needed - already_allocated;
  int free_blocks = 0;
  for (int i = data_start; i < sb.number_blocks; i++) {
    if (fat[i] == 0) free_blocks++;
  }

  if (free_blocks < new_blocks_needed) {
    errno = ENOSPC;
    return -1;
  }

  // Alocar primeiro bloco se necessário
  if (dir[arq_encontrado].first == EOFF) {
    int first = -1;
    for (int i = data_start; i < sb.number_blocks; i++) {
      if (fat[i] == 0) {
        first = i;
        fat[i] = EOFF;
        dir[arq_encontrado].first = first;
        break;
      }
    }
    if (first == -1) {
      errno = ENOSPC;
      return -1;
    }
    current = first;
  }

  // Caminhar até o bloco de início do offset
  int skip = offset / BLOCK_SIZE;
  for (int i = 0; i < skip; i++) {
    if (current >= sb.number_blocks) {
      errno = EINVAL;
      return -1;
    }
    if (fat[current] == EOFF) {
      // Aloca novo bloco
      int novo = -1;
      for (int j = data_start; j < sb.number_blocks; j++) {
        if (fat[j] == 0) {
          novo = j;
          break;
        }
      }
      if (novo == -1) {
        errno = ENOSPC;
        return -1;
      }
      fat[current] = novo;
      fat[novo] = EOFF;
    }
    current = fat[current];
  }

  // Escrita nos blocos
  int bytes_written = 0;
  int block_offset = offset % BLOCK_SIZE;
  char temp_block[BLOCK_SIZE];

  while (bytes_written < writable) {
    if (current >= sb.number_blocks) {
      errno = EINVAL;
      return -1;
    }
    ds_read(current, temp_block);

    int start = (bytes_written == 0) ? block_offset : 0;
    int space = BLOCK_SIZE - start;
    int to_copy = (writable - bytes_written < space) 
                  ? writable - bytes_written 
                  : space;

    memcpy(temp_block + start, buff + bytes_written, to_copy);
    ds_write(current, temp_block);

    bytes_written += to_copy;

    if (bytes_written < writable) {
      if (fat[current] == EOFF) {
        int novo = -1;
        for (int i = data_start; i < sb.number_blocks; i++) {
          if (fat[i] == 0) {
            novo = i;
            break;
          }
        }
        if (novo == -1) {
          errno = ENOSPC;
          return bytes_written;
        }
        fat[current] = novo;
        fat[novo] = EOFF;
      }
      current = fat[current];
    }
  }

  // Atualizar tamanho do arquivo, se necessário
  if (offset + bytes_written > dir[arq_encontrado].length) {
    dir[arq_encontrado].length = offset + bytes_written;
  }

  // Write FAT and directory to disk
  for (int i = 0; i < sb.n_fat_blocks; i++) {
    ds_write(TABLE + i, ((char *)fat) + i * BLOCK_SIZE);
  }

  ds_write(DIR, (char *)dir);

  return bytes_written;
}