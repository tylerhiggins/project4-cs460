#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <dirent.h>

#include "BackItUp.h"

#define DEBUG 1
#define BDIR "testdir/.backup"

void printError(char* error){
	if( DEBUG ){
		printf("Error: %s\n", error);
	}
}

int createBackupDir(){
	int err = mkdir(BDIR, 0777);
	if( err == -1 ){
		if( errno == EEXIST ){
			printf("Backup directory already exists.\n");
			return 0;
		}else{
			printError(strerror(errno));
		}
	}
	return 0;
}

int copyFile(FILE *fp, char* fname){
	//make a new file
	char dest[256] = "testdir/.backup/";
	strncat(dest, fname, sizeof(fname));

	if( DEBUG ){
		printf("Making copy at %s\n", dest);
	}

	//create a new file for writing
	FILE *new = fopen(dest, "w+");
	if( new == NULL ){
		printError(strerror(errno));
		return 1;
	}
	while( 1 ){
		char b;
		int read = fread(&b, 1, 1, fp);

		if( read <= 0 ){
			break;
		}

		fwrite(&b, 1, 1, new);
	}

	fclose(new);
}

int recursiveCopy( char* dname ){
	//travel through all directories and copy files
	// into the same directory structure
	struct dirent* ds;
	DIR* dir = opendir(dname);
	while( (ds = readdir(dir)) != NULL ){
		//while the next directory is not null
		if( strncmp( ds->d_name, ".", 1 ) != 0 && strncmp(ds->d_name, "..", 2) != 0 ){
			//and this is not the current or previous directory structure
			//check status of object
			struct stat st;
			
			char fname[256] = "";
			strcat(fname, dname);
			strcat( fname, "/" );
			strncat(fname, ds->d_name, sizeof(ds->d_name));

			//treating symlinks as symlinks, not the files they link to
			int err = lstat(fname, &st);
			if( err == -1 ){
				printError(strerror(errno));
				return 1;
			}
			//check if this is a regular file
			if( S_ISREG( st.st_mode ) ){
				//if so, copy the file to the backup directory
				//open just for reading
				FILE* fp = fopen(fname, "r");
				if( fp == NULL ){
					printError(strerror(errno));
					return 1;
				}
				copyFile(fp, ds->d_name);
				fclose(fp);
			}else if( S_ISDIR( st.st_mode ) ){
				printf("TODO: skipping directory\n");
			}
		}
	}
	closedir(dir);
}


int main(int argc, char **argv) {

	createBackupDir();

	recursiveCopy("testdir");

	return 0;
}



