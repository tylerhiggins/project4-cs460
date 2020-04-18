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

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t counter_lock = PTHREAD_MUTEX_INITIALIZER;

int totalBytes = 0;			// shared
int successfulFiles = 0;	// shared
int total_threads = 0;		// shared

// TODO add error checks for lock/unlock
void updateTotalBytes(int b) {
	pthread_mutex_lock(&lock);
	totalBytes += b;
	successfulFiles++;
	pthread_mutex_unlock(&lock);
}

// TODO add error checks for lock/unlock
/*
Updates the thread count and stores the threads number
*/
void updateThreadCount(struct thread_args args) {
	pthread_mutex_lock(&lock);
	args.threadNum = total_threads;
	total_threads++;
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
			printf("[  main  ] Backup directory already exists.\n");
			return 0;
		}else{
			perror("createBackupDir");
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
		perror("createBackupFile");
		exit(-1);
	}
	if (DEBUG) { printf("[thread %d] Opened file: %s\n", args.threadNum, args.filename );}

	int exists = access( args.destination, F_OK ) != -1;
	int canCopy = 1;
	if( exists ){
		if( DEBUG ){
			printf("[thread %d] Backup file already exists, checking modification times.\n", args.threadNum);
		}
		struct stat testSt;
		int err = lstat(args.destination, &testSt);
		if( err == -1 ){
			perror("createBackupFile");
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
		printf("[thread %d] Copied %d bytes from %s to %s\n", args.threadNum, bytes, args.filename, args.destination);
	}else if( exists ){
		printf("[thread %d] NOTICE: %s is already the most current version\n", args.threadNum, args.filename);
	}
	
	fclose(fp);
	pthread_exit(NULL);
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
		perror("copyFile");
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
	
	int num_threads = 0;
	int num_files = 0;
	num_files = countFiles(dname);
	if (DEBUG) printf("[  main  ] counted %d files\n", num_files);
	pthread_t thread_list[num_files];
	// pthread_t *thread_list;
	// thread_list = (pthread_t*) malloc(sizeof(pthread_t) * num_files);

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
				perror("recursiveCopy");
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
				num_threads++;
				args.threadNum = total_threads;
				if (DEBUG) printf("[  main  ] creating thread %d to copy %s\n", total_threads, args.filename);
				total_threads++;
				// call the thread
				pthread_t copy;
				pthread_create(&copy, NULL, createBackupFile, &args);
				// thread_list[num_threads] = copy;
				pthread_create(&thread_list[num_threads-1], NULL, createBackupFile, &args);

				pthread_join(copy, NULL);
				// printf("thread %d joined\n", num_threads);
				// threadList[thread_count] = copy;
				// thread_count++;
				// updateThreadCount(args);
				// printf("[thread %d] created\n", args.threadNum);
				// pthread_create(&thread_list[num_threads], NULL, createBackupFile, &args);
				// num_threads++;

			}
			else if( S_ISDIR( st.st_mode ) ){
				if (DEBUG) printf("[  main  ] TODO: skipping directory '%s'\n", fname);
			}
		}
	}
	closedir(dir);
	// printf("[main 0] joining %d threads\n", num_threads);
	// joinThreads(thread_list, num_threads);
	// printf("[main 0] finished joining threads %d\n", num_threads);
	// printf("Freeing thread_list\n");
	// free(thread_list);
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
			perror("recursiveRestore");
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
					perror("recursiveRestore");
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
					perror("recursiveRestore");
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
				perror("countFiles");
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

// JOIN threads list
void joinThreads(pthread_t thread_list[], int count) {
	alarm(1);
	pause();
	for (int i = 0; i < count; i++) {
		if (DEBUG) printf("[thread %d] waiting to join\n", i);
		pthread_join(thread_list[i], NULL);
		if (DEBUG) printf("[thread %d] has joined\n", i);
	}

}

int main(int argc, char **argv) {
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
			printf("[  main  ] Restoring from backup.\n");
		}
		// num = countFiles(restoreDirectory);
		// threadList = (pthread_t*) malloc(sizeof(pthread_t) * num);
		recursiveRestore(restoreDirectory);

	}else{
		if( createBackupDir() ){
			return 1;
		}
		
		// num = countFiles(backupDirectory);
		// threadList = (pthread_t*) malloc(sizeof(pthread_t) * num);
		// if (DEBUG) printf("Counted %d files\n", num);
		if(recursiveCopy(backupDirectory)){
			return 1;
		}
	}
	// joinThreads(num);
	// free(threadList);

	printf("[  main  ] Success\n");
	return 0;
}


