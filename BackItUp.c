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
#define MAXTHREADS 1000

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int count = 0;						// shared
pthread_t threads[MAXTHREADS];		// shared


// save the thread id to a list
void storeThreadID(pthread_t id) {
	if( pthread_mutex_lock(&mutex) ){
		printf("Error: mutex_lock reports %s\n", strerror(errno) );
		exit(1);
	}
	if (DEBUG) printf("Storing thread %d, count %d\n", id, count);
	threads[count] = id;
	count++;
	if( pthread_mutex_unlock(&mutex) ){
			printf("Error: mutex_unlock reports %s\n", strerror(errno) );
			exit(1);
		}
}

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


/* 
This method is handed off to a thread,
opens the original file for reading
and backs up the file by making a copy
*/
void * createBackupFile(void *argument) {
	// load in the struct
	struct thread_args args = *(struct thread_args*)argument;
	if (DEBUG) { printf("\nThread %d creating backup of: %s\n", pthread_self(), args.filename);}

	//open the file just for reading
	FILE* fp = fopen(args.filename, "r");
	if( fp == NULL ){
		printError(strerror(errno));
		exit(-1);
	}
	if (DEBUG) { printf("Opened file: %s\n", args.filename );}

	int exists = access( args.destination, F_OK ) != -1;
	int canCopy = 1;
	if( exists ){
		if( DEBUG ){
			printf("Backup file already exists, checking modification times.\n");
		}
		struct stat testSt;
		int err = lstat(args.destination, &testSt);
		if( err == -1 ){
			printError(strerror(errno));
			exit(-1);
		}
		//true if the backup file was last modified before the main file
		canCopy = testSt.st_mtime < args.modifiedTime;
	}

	// copy the file to the backup directory
	if( canCopy ){
		if( exists ){
			printf("Overwriting outdated backup file.\n");
		}
		if (DEBUG) { printf("Copying file: %s to %s\n", args.filename, args.destination);}
		copyFile(fp, args.destination);	
	}else if( exists ){
		printf("Backup file is already up-to-date.\n");
	}
	fclose(fp);
}


// writes a copy of fp to fname
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
	if (DEBUG) printf("File copied successfully\n");
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
			strncat(fname, dname, strlen(fname) + strlen(dname) + 1);
			strncat( fname, "/", strlen(fname) + 2);
			strncat(fname, ds->d_name, strlen(fname) + strlen(ds->d_name) + 1);

			//treating symlinks as symlinks, not the files they link to
			int err = lstat(fname, &st);
			if( err == -1 ){
				printError(strerror(errno));
				return 1;
			}

			if (DEBUG) printf("Checking file type: %s\n", fname);
			//check if this is a regular file
			if( S_ISREG( st.st_mode ) ){
				if (DEBUG) printf("%s is Regular\n", fname);
				//make a new filename for the copy
				char dest[256] = "testdir/.backup/";
				strncat(dest, ds->d_name, strlen(dest) + strlen(ds->d_name) + 1);
				strncat(dest, ".bak", strlen(dest) + strlen(".bak") + 1);

				// store variables in struct to avoid sharing memory
				struct thread_args args;
				strncpy(args.filename, fname, strlen(fname));
				strncpy(args.destination, dest, strlen(dest));
				args.modifiedTime = st.st_mtime;

				if (DEBUG) printf("Thread %d spinning up child thread\n", pthread_self());
				// call the thread
				pthread_t copy;
				if (pthread_create(&copy, NULL, createBackupFile, &args) < 0) {
					printError(strerror(errno));
					exit(-1);
				}
				storeThreadID(copy);
				if (DEBUG) printf("Thread %d continuing...\n", pthread_self());
				// pthread_join(copy, NULL);
				// pthread_detach(copy);
				// if (DEBUG) printf("detached thread\n");
			}
			else if( S_ISDIR( st.st_mode ) ){
				if (DEBUG) printf("%s is a Directory\n", fname);
				printf("TODO recurse into Directory: %s\n", fname);
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
	
	// lock because count is a shared variable
	if( pthread_mutex_lock(&mutex) ){
		printf("Error: mutex_lock reports %s\n", strerror(errno) );
		exit(1);
	}
	int numThreads = count;
	if( pthread_mutex_unlock(&mutex) ){
		printf("Error: mutex_lock reports %s\n", strerror(errno) );
		exit(1);
	}

	// join the threads
	for(int i = 0; i < numThreads; i++) {
		if( pthread_mutex_lock(&mutex) ){
			printf("Error: mutex_lock reports %s\n", strerror(errno) );
			exit(1);
		}
		if (DEBUG) printf("Joining thread %ld\n", threads[i]);
		pthread_join(threads[i], NULL);
		if( pthread_mutex_unlock(&mutex) ){
			printf("Error: mutex_lock reports %s\n", strerror(errno) );
			exit(1);
		}
	}


	pthread_mutex_destroy(&mutex);

	return 0;
}



