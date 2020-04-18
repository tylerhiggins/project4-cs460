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

int totalBytes = 0;
int successfulFiles = 0;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

pthread_t *threadList;
int thread_counter = 0;

void updateTotalBytes(int b) {
	pthread_mutex_lock(&lock);
	totalBytes += b;
	successfulFiles++;
	pthread_mutex_unlock(&lock);
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
	int bytes = 0;
	// load in the struct
	struct thread_args args = *(struct thread_args*)argument;
	printf("[thread %d] Backing up %s\n", args.threadNum, args.filename);

	//if so, copy the file to the backup directory
	//open just for reading
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

	if( canCopy ){
		if( exists ){
			printf("[thread %d] WARNING: Overwriting %s\n", args.threadNum, args.filename);
		}
		if (DEBUG) { printf("Copying file: %s to %s\n", args.filename, args.destination);}
		bytes = copyFile(fp, args.destination);	
		updateTotalBytes(bytes);
		printf("[thread %d] Copied %d bytes from %s to %s\n", args.threadNum, bytes, args.filename, args.destination);
	}else if( exists ){
		printf("Backup file is already up-to-date.\n");
	}
	
	fclose(fp);
	printf("[thread %d] Exiting\n", args.threadNum);
	// pthread_exit(NULL);
}


// writes a copy of fp to fname
int copyFile(FILE *fp, char* fname){
	int bytes_copied = 0;
	if( DEBUG ){
		printf("Making copy at %s\n", fname);
	}

	//create a new file for writing
	FILE *new = fopen(fname, "w+");
	if( new == NULL ){
		printError(strerror(errno));
		return -1;
	}
	while( 1 ){
		char b;
		int read = fread(&b, 1, 1, fp);

		if( read <= 0 ){
			break;
		}

		fwrite(&b, 1, 1, new);
		bytes_copied++;
	}

	fclose(new);
	return bytes_copied;
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
			strncat(fname, dname, strlen(dname));
			strcat( fname, "/");
			strncat(fname, ds->d_name, strlen(ds->d_name));

			//treating symlinks as symlinks, not the files they link to
			int err = lstat(fname, &st);
			if( err == -1 ){
				printError(strerror(errno));
				return 1;
			}
			//check if this is a regular file
			if( S_ISREG( st.st_mode ) ){

				//make a new filename for the copy
				char dest[256] = "testdir/.backup/";
				strncat(dest, ds->d_name, strlen(ds->d_name));
				strcat(dest, ".bak");

				// store variables in struct to avoid sharing memory
				struct thread_args args;
				
				strncpy(args.filename, fname, strlen(fname) + 1);
				strncpy(args.destination, dest, strlen(dest) + 1);

				args.modifiedTime = st.st_mtime;
				args.threadNum = thread_counter;

				// call the thread
				// pthread_t copy;
				pthread_create(&threadList[thread_counter], NULL, createBackupFile, &args);
				printf("[main] thread %d created\n", thread_counter);
				// pthread_join(threadList[thread_counter], NULL);
				thread_counter++;

			}
			else if( S_ISDIR( st.st_mode ) ){
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


void *restoreThread(void *arg){

	struct restore_args args = *(struct restore_args*)arg;
	char destination[256];
	strncpy(destination,args.destination,strlen(args.destination));
	char *filename = NULL;
	char *tmp;
	char *rest = destination;
	// Grabbing just the filename from the path
	while((tmp = strtok_r(rest,"/",&rest))){
		filename = tmp;
	}
	// Before copy
	printf("[thread %d] Backing up %s\n",args.threadNum,filename);
	int bytes = copyFile(args.fileToRestore,args.destination);
	// If successful, display message, else display error
	if(bytes != -1){
		printf("[thread %d] Copied %d bytes from %s.bak to %s\n", args.threadNum,bytes,
			filename,filename);
			updateTotalBytes(bytes);
	} else {
		printf("[thread %d] ERROR: could not copy %s.bak to %s\n", args.threadNum, filename,filename);
	} 
}


int recursiveRestore( char* dname ){
	//similar to recursive copy, only moving files from the 
	// backup directory to the main directory
	int num_threads = 0;
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
				} else{
				//compare modification times
					canCopy = (backup.st_mtime < tmp.st_mtime);
				}
			}

			if( canCopy ){
				if( DEBUG ){
					printf("Restoring file.\n");
				}
				//perform the copy
				struct restore_args args;
				//open file for reading only
				FILE *fp = fopen(fname, "r");
				if( fp == NULL ){
					printError(strerror(errno));
					return 1;
				}
				args.fileToRestore = fp;
				strncpy(args.destination,newDest,strlen(newDest));
				if(DEBUG){
					printf("args.destination: %s\n",args.destination);
				}
				num_threads++;
				args.threadNum = num_threads;
				void *ret;
				pthread_t restoreT;
				pthread_create(&restoreT,NULL,restoreThread,&args);
				pthread_join(restoreT,NULL);

			}else if( DEBUG ){
				printf("Not restoring newer or up-to-date backup file.\n");
			}

		}else if( S_ISDIR( backup.st_mode ) ){
			printf("TODO: skipping directory restoration\n");
		}

	}
	printf("Copied %d files (%d bytes)\n",successfulFiles, totalBytes);
	pthread_mutex_destroy(&lock);

}

// recursively traverse the directory and counts the number of files
int countFiles(char* dname) {
	int count = 0;
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
				printf("countFiles %s\n", strerror(errno));
				return 1;
			}
			//check if this is a regular file
			if( S_ISREG( st.st_mode ) ){
				count++;
			}
			else if( S_ISDIR( st.st_mode ) ){
				// printf("Entering directory %s\n", fname);
				count += countFiles(fname);
			}
		}
	}
	closedir(dir);
	return count;
}

// // JOIN threads list
void joinThreads(int count) {
	for (int i = 0; i < count; i++) {
		if (DEBUG) printf("[thread %d] is joining, %ld\n", i, threadList[i]);
		pthread_join(threadList[i], NULL);
	}
	if (DEBUG) printf("finished joining all threads.\n");
}




int main(int argc, char **argv) {
	int num = 0;
	char * backupDirectory = "testdir";
	char * restoreDirectory = "testdir/.backup";

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
		num = countFiles(restoreDirectory);
		threadList = (pthread_t*) malloc(sizeof(pthread_t) * num);
		recursiveRestore(restoreDirectory);

	}else{
		if( createBackupDir() ){
			return 1;
		}
		
		num = countFiles(backupDirectory);
		threadList = (pthread_t*) malloc(sizeof(pthread_t) * num);
		if (DEBUG) printf("Counted %d files\n", num);
		if(recursiveCopy(backupDirectory)){
			return 1;
		}
	}
	printf("starting join\n");

	joinThreads(num);
	free(threadList);

	printf("Success\n");

	return 0;
}




