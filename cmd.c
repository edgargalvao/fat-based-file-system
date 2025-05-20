#include "ds.h"
#include "fat.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Function prototypes for file import/export between Linux and the simulated file system
int cpout( char * os_path,  char *name );
int cpin( char *name, char * os_path);

// Main function: command-line interface for interacting with the simulated FAT file system
int main( int argc, char *argv[] )
{
	char line[1024];    // Buffer for user input
	char cmd[1024];     // Buffer for command
	char arg1[1024];    // Buffer for first argument
	char arg2[1024];    // Buffer for second argument
	int result, args;   // Variables for command results and argument count

	// Check for correct number of command-line arguments
	if(argc!=3) {
		printf("uso: %s <arquivo> <quantosblocos>\n",argv[0]);
		return 1;
	}

	// Initialize disk simulation with the given file and number of blocks
	if(!ds_init(argv[1],atoi(argv[2]))) {
		printf("falha %s: %s\n",argv[1],strerror(errno));
		return 1;
	}

	printf("simulacao de disco %s com %d blocos\n",argv[1],ds_size());

	// Main command loop: prompt user for commands until "sair" is entered
	while(1) {
		printf(" sys> ");
		fflush(stdout);

		// Read user input
		if(!fgets(line,sizeof(line),stdin)) break;

		if(line[0]=='\n') continue;
		line[strlen(line)-1] = 0;

		// Parse the user input into command and up to two arguments
		args = sscanf(line,"%s %s %s",cmd,arg1,arg2);
		if(args==0) continue;

		// Handle each supported command
		if(!strcmp(cmd,"formatar")) {
			// Format the simulated disk
			if(args==1) {
				if(!fat_format()) {
					printf("formatou\n");
				} else {
					printf("falhou na formatacao!\n");
				}
			} else {
				printf("uso: formatar\n");
			}
		} else if(!(strcmp(cmd,"montar"))) {
			// Mount the FAT file system
			if(args==1) {
				if(!fat_mount()) {
					printf("montagem ok\n");
				} else {
					printf("falha de montagem!\n");
				}
			} else {
				printf("uso: montar\n");
			}
		} else if(!strcmp(cmd,"depurar")) {
			// Debug: print file system state
			if(args==1) {
				fat_debug();
			} else {
				printf("uso: depurar\n");
			}
		} else if(!strcmp(cmd,"medir")) {
			// Get file size
			if(args==2) {
				result = fat_getsize(arg1);
				if(result>=0) {
					printf("o arquivo %s mede %d\n",arg1,result);
				} else {
					printf("falha na medida!\n");
				}
			} else {
				printf("uso: medir <arquivo>\n");
			}
			
		} else if(!strcmp(cmd,"criar")) {
			// Create a new file
			if(args==2) {
				result = fat_create(arg1);
				if(result==0) {
					printf("novo arquivo %s\n",arg1);
				} else {
					printf("falha ao criar arquivo!\n");
				}
			} else {
				printf("uso: criar <arquivo>\n");
			}
		} else if(!strcmp(cmd,"deletar")) {
			// Delete a file
			if(args==2) {
				if(!fat_delete(arg1)) {
					printf("arquivo %s deletado\n",arg1);
				} else {
					printf("falha na delecao!\n");	
				}
			} else {
				printf("uso: deletar <arquivo>\n");
			}
		} else if(!strcmp(cmd,"ver")) {
			// View file contents (output to stdout)
			if(args==2) {
				if(!cpout(arg1,"/dev/stdout")) {
					printf("falha em ver arquivo!\n");
				}
			} else {
				printf("uso: ver <nome>\n");
			}

		} else if(!strcmp(cmd,"importar")) {
			// Import a file from Linux into the simulated file system
			if(args==3) {
				if(cpin(arg1,arg2)) {
					printf("arquivo linux %s copiado para %s\n",arg1,arg2);
				} else {
					printf("falha ao copiar!\n");
				}
			} else {
				printf("uso: importar <nome no linux> <nome fat-sys>\n");
			}

		} else if(!strcmp(cmd,"exportar")) {
			// Export a file from the simulated file system to Linux
			if(args==3) {
				if(cpout(arg1,arg2)) {
					printf("fat-sys %s copiado para arquivo %s\n", arg1,arg2);
				} else {
					printf("falha ao copiar!\n");
				}
			} else {
				printf("uso: exportar <nome fat-sys> <nome linux>\n");
			}

		} else if(!strcmp(cmd,"help")) {
			// Print help message
			printf("Comandos:\n");
			printf("    formatar\n");
			printf("    montar\n");
			printf("    depurar\n");
			printf("    criar	<arquivo>\n");
			printf("    deletar <arquivo>\n");
			printf("    ver     <arquivo>\n");
			printf("    medir   <arquivo>\n");
			printf("    importar <nome no linux> <nome fat-sys>\n");
			printf("    exportar <nome fat-sys> <nome no linux>\n");
			printf("    help\n");
			printf("    sair\n");
		} else if(!strcmp(cmd,"sair")) {
			// Exit the command loop
			break;
		} else {
			// Unknown command
			printf("comando desconhecido: %s\n",cmd);
			printf("digite 'help'.\n");
			result = 1;
		}
	}

	printf("fechando o disco simulado\n");
	ds_close();

	return 0;
}

// Import a file from Linux into the simulated file system
int cpin( char *name, char *op_path )
{
	FILE *file;
	int offset=0, result, actual;
	char buffer[16384];

	file = fopen(name,"r"); // Open Linux file for reading
	if(!file) {
		printf("falha ao acessar %s: %s\n",name,strerror(errno));
		return 0;
	}

	// Read from Linux file and write to simulated file system
	while(1) {
		result = fread(buffer,1,sizeof(buffer),file);
		if(result<=0) break;
		if(result>0) {
			actual = fat_write(op_path,buffer,result,offset);
			if(actual<0) {
				printf("ERRO: fat_write returnou codigo %d\n",actual);
				break;
			}
			offset += actual;
			if(actual!=result) {
				printf("ATENCAO: fat_write escreveu apenas %d bytes, em vez de %d bytes\n",actual,result);
				break;
			}
		}
	}

	printf("copia de %d bytes\n",offset);

	fclose(file);
	return 1;
}

// Export a file from the simulated file system to Linux
int cpout( char *os_path, char *name )
{
	FILE *file;
	int offset=0, result;
	char buffer[16384];
    if(strcmp(name,"/dev/stdout"))
		file = fopen(name,"w"); // Open Linux file for writing
	else
		file = stdout;         // Or use stdout for "ver" command
	if(!file) {
		printf("nao deu para abrir %s: %s\n",name,strerror(errno));
		return 0;
	}

	// Read from simulated file system and write to Linux file
	while(1) {
		result = fat_read(os_path,buffer,sizeof(buffer),offset);
		if(result<=0) break;
		fwrite(buffer,1,result,file);
		offset += result;
	}

	printf("copia de %d bytes\n",offset);

	if(file != stdout)
		fclose(file);
	return 1;
}
