#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ds.h"

// Global variables to keep track of disk state and statistics
static int number_blocks=0;    // Total number of blocks in the disk
static int number_reads=0;     // Number of read operations performed
static int number_writes=0;    // Number of write operations performed
static FILE *disk;             // File pointer simulating the disk

// Returns the total number of blocks in the disk
int ds_size()
{
	return number_blocks;
}

// Initializes the disk simulation with the given filename and number of blocks
int ds_init( const char *filename, int n )
{
	disk = fopen(filename,"r+");         // Try to open existing file
	if(!disk) disk = fopen(filename,"w+"); // If not exist, create new file
	if(!disk) return 0;                    // Return 0 on failure

	ftruncate(fileno(disk),n * BLOCK_SIZE); // Set file size to n blocks

	number_blocks = n;    // Store number of blocks
	number_reads = 0;     // Reset read counter
	number_writes = 0;    // Reset write counter

	return 1; // Success
}

// Checks for valid block number and buffer pointer
static void check( int number, const void *buff )
{
	if(number<0) {
		printf("ERROR: blocknum (%d) is negative!\n",number);
		abort();
	}

	if(number>=number_blocks) {
		printf("ERROR: blocknum (%d) is too big!\n",number);
		abort();
	}

	if(!buff) {
		printf("ERROR: null data pointer!\n");
		abort();
	}
}

// Reads a block from disk into the buffer
void ds_read( int number, char *buff )
{
	int x;
	check(number,buff); // Validate block number and buffer pointer
	fseek(disk,number*BLOCK_SIZE,SEEK_SET); // Move file pointer to correct block
	x = fread(buff,BLOCK_SIZE,1,disk);      // Read one block into buffer
	if(x==1) {
		number_reads++; // Increment read counter if successful
	} else {
		printf("disk simulation failed\n");
		perror("ds");
		exit(1); // Exit on failure
	}
}

// Writes a block from buffer to disk
void ds_write( int number, const char *buff )
{
	int x;
	check(number,buff); // Validate block number and buffer pointer
	fseek(disk,number*BLOCK_SIZE,SEEK_SET); // Move file pointer to the correct block position
	x = fwrite(buff,BLOCK_SIZE,1,disk); // Write one block of data from buffer to disk
	if(x==1) {
		number_writes++; // Increment write counter if successful
	} else {
		printf("disk simulation failed\n");
		perror("ds");
		exit(1); // Exit on failure
	}
}

// Closes the disk and prints statistics
void ds_close()
{
	printf("%d reads\n",number_reads);   // Print total number of reads
	printf("%d writes\n",number_writes); // Print total number of writes
	fclose(disk);                        // Close the disk file
}

