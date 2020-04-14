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
			return 1;
		}
	}
	if( DEBUG ){
		printf("Backup directory does not exist, creating.\n");
	}
	return 0;
}

int copyFile(FILE *fp, char* fname){

	if( DEBUG ){
		printf("Making copy at %s\n", fname);
	}

	//create a new file for writing
	FILE *new = fopen(fname, "w+");
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
			strncat(fname, ds->d_name, strlen(ds->d_name));

			//treating symlinks as symlinks, not the files they link to
			int err = lstat(fname, &st);
			if( err == -1 ){
				printError(strerror(errno));
				return 1;
			}
			//check if this is a regular file
			if( S_ISREG( st.st_mode ) ){
				if( DEBUG ){
					printf("Working on file %s\n", fname );
				}
				//if so, copy the file to the backup directory
				//open just for reading
				FILE* fp = fopen(fname, "r");
				if( fp == NULL ){
					printError(strerror(errno));
					return 1;
				}
				//make a new file for the copy
				char dest[256] = "testdir/.backup/";
				strncat(dest, ds->d_name, strlen(ds->d_name));
				strcat(dest, ".bak");

				int exists = access( dest, F_OK ) != -1;
				int canCopy = 1;

				if( exists ){
					if( DEBUG ){
						printf("Backup file already exists, checking modification times.\n");
					}
					struct stat testSt;
					err = lstat(dest, &testSt);
					if( err == -1 ){
						printError(strerror(errno));
						return 1;
					}
					//true if the backup file was last modified before the main file
					canCopy = testSt.st_mtime < st.st_mtime;
				}

				if( canCopy ){
					if( exists ){
						printf("Overwriting outdated backup file.\n");
					}
					copyFile(fp, dest);	
				}else if( exists ){
					printf("Backup file is already up-to-date.\n");
				}
				
				fclose(fp);
			}else if( S_ISDIR( st.st_mode ) ){
				printf("TODO: skipping directory\n");
			}
		}
	}
	closedir(dir);
}

int backupToMainPath( char* result, char* dirName, char* fileName ){
	//assume the file name has a .bak extension
	strncat(result, dirName, strlen(dirName)-7);
	strncat(result, fileName, strlen(fileName)-4);
	return 0;
}

int recursiveRestore( char* dname ){
	//similar to recursive copy, only moving files from the 
	// backup directory to the main directory
	DIR *backupDir = opendir(dname);
	struct dirent *backupDirent;

	while( (backupDirent = readdir(backupDir)) != NULL ){
		//check the current object
		if( strncmp( backupDirent->d_name, ".", 1 ) == 0 || strncmp(backupDirent->d_name, "..", 2) == 0 ){
			//skip the current and previous directory notation
			continue;
		}


		//prepend the working directory to the file name
		char fname[256] = "";
		strcat(fname, dname);
		strcat(fname, "/");
		strncat(fname, backupDirent->d_name, strlen(backupDirent->d_name));

		struct stat backup;
		int err = lstat(fname, &backup);
		if( err == -1 ){
			printError(strerror(errno));
			return 1;
		}

		//check if it is a regular file
		if( S_ISREG( backup.st_mode ) ){
			if( DEBUG ){
				printf("Working on file %s\n", fname);
			}

			//copy the regular file to the main directory
			//check if the file already exists and/or is newer than the backup
			int canCopy = 1;
			char newDest[256] = "";
			if( access(fname, F_OK) != -1 ){
				//file exists
				struct stat tmp;
				
				backupToMainPath(newDest, dname, backupDirent->d_name);

				err = lstat(newDest, &tmp);
				if( err == -1 ){
					printError(strerror(errno));
					return 1;
				}

				//compare modification times
				canCopy = (backup.st_mtime < tmp.st_mtime);
			}

			if( canCopy ){
				if( DEBUG ){
					printf("Restoring file.\n");
				}
				//perform the copy

				//open file for reading only
				FILE *fp = fopen(fname, "r");
				if( fp == NULL ){
					printError(strerror(errno));
					return 1;
				}
				copyFile(fp, newDest);

			}else if( DEBUG ){
				printf("Not restoring newer or up-to-date backup file.\n");
			}

		}else if( S_ISDIR( backup.st_mode ) ){
			printf("TODO: skipping directory restoration\n");
		}

	}

}


int main(int argc, char **argv) {

	int restore = 0;
	for( int i = 0; i < argc; i++ ){
		if( strncmp(argv[i], "-r", 2) == 0 ){
			restore = 1;
			break;
		}
	}

	if( restore ){
		if( DEBUG ){
			printf("Restoring from backup.\n");
		}
		recursiveRestore("testdir/.backup");

	}else{
		if( createBackupDir() ){
			return 1;
		}

		if(recursiveCopy("testdir")){
			return 1;
		}
	}

	return 0;
}



