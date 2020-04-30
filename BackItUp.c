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

#define DEBUG 0
#define DEBUG2 0
#define BACKUP_DIR "./.backup"

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t counter_lock = PTHREAD_MUTEX_INITIALIZER;

int totalBytes = 0;			// shared
int successfulFiles = 0;	// shared
int total_threads = 0;		// shared

// updates the total number of bytes copied by the program
void updateTotalBytes(int b){
	if (pthread_mutex_lock(&lock) < 0) perror("Failed to lock mutex");
	totalBytes += b;
	successfulFiles++;
	if (pthread_mutex_unlock(&lock) < 0) perror("Failed to unlock mutex");
}

// Updates the thread count and stores the threads number
void updateThreadCount(struct copy_args args){
	if (pthread_mutex_lock(&lock) < 0) perror("Failed to lock mutex");
	args.threadNum = total_threads;
	total_threads++;
	if (pthread_mutex_unlock(&lock) < 0) perror("Failed to unlock mutex");
}

// Prints an error message
void printError(char* error){ 
	if (DEBUG){
		printf("Error: %s\n", error);
	}
}
// Creates the backup directory if it doesn't already exist
int createBackupDir(){
	int err = mkdir(BACKUP_DIR, 0777);
	if (err == -1){
		if (errno == EEXIST){
			if (DEBUG) printf("[  main  ] Backup directory already exists.\n");
			return 0;
		} else{
			perror("createBackupDir");
			return 1;
		}
	}
	if (DEBUG){
		printf("Backup directory does not exist, creating.\n");
	}
	return 0;
}

// Checks to see if the directory already exists, creates it if it doesn't
int checkDir(char *dir){
	int err = mkdir(dir, 0777);
	if (err == -1){
		if (errno == EEXIST)
			return 2;
		else {
			perror("checkDir");
			return 1;
		}
	}
	return 0;
}

/* 
This method is handed off to a thread,
opens the original file for reading
and backs up the file by making a copy
*/
void * backupThread(void *argument){
	int bytes = 0;
	// load in the struct
	struct copy_args args = *(struct copy_args*)argument;
	printf("[thread %d] Backing up %.*s\n", args.threadNum, strlen(args.filename) - 2, args.filename + 2);

	//if so, copy the file to the backup directory
	//open just for reading
	FILE* fp = fopen(args.filename, "r");
	if (fp == NULL){
		perror("backupThread");
		exit(-1);
	}
	if (DEBUG){ printf("[thread %d] Opened file: %s\n", args.threadNum, args.filename);}

	int exists = access(args.destination, F_OK) != -1;
	int canCopy = 1;
	if (exists){
		if (DEBUG){
			printf("[thread %d] Backup file already exists, checking modification times.\n", args.threadNum);
		}
		struct stat testSt;
		int err = lstat(args.destination, &testSt);
		if (err == -1){
			perror("backupThread");
			exit(-1);
		}
		//true if the backup file was last modified before the main file
		canCopy = testSt.st_mtime < args.modifiedTime;
	}
	if (canCopy){
		if (exists){
			printf("[thread %d] WARNING: Overwriting %.*s\n", args.threadNum, strlen(args.filename) - 2, args.filename + 2);
		}
		if (DEBUG){ printf("Copying file: %s to %s\n", args.filename, args.destination);}
		bytes = copyFile(fp, args.destination);	
		if (bytes > 0){
			updateTotalBytes(bytes);
		}
		printf("[thread %d] Copied %d bytes from %.*s to %.*s\n", args.threadNum, bytes, strlen(args.filename) - 2, args.filename + 2, strlen(args.destination) - 2, args.destination + 2);
	} else if (exists){
		printf("[thread %d] NOTICE: %.*s is already the most current version\n", args.threadNum, strlen(args.filename) - 2, args.filename + 2);
	}
	fclose(fp);
	pthread_exit(NULL);
}

// writes a copy of fp to fname
int copyFile(FILE *fp, char* fname){
	int bytes_copied = 0;
	if (DEBUG) printf("[thread _] copying %s\n", fname);

	//create a new file for writing
	FILE *new = fopen(fname, "w+");
	if (new == NULL){
		fprintf(stderr, "copyFile: %s '%s'\n", strerror(errno), fname);
		return -1;
	}
	while(1){
		char b;
		int read = fread(&b, 1, 1, fp);
		if (read <= 0){
			break;
		}
		fwrite(&b, 1, 1, new);
		bytes_copied++;
	}
	fclose(new);
	return bytes_copied;
}

// removes the top level directory from a filepath
char* removeParentDir(char* path){
	char *new_path = (char*) calloc(PATH_MAX, sizeof(char));
	new_path[0] = '\0';		// just incase path is empty
	int slash = 0;
	// Grabs just the filename from the path
	for (int i = 0; i < strlen(path); i++){
		if (path[i] == '/'){
			slash = i;
			break;
		} 
	}
	strncpy(new_path, path+slash, strlen(path) - slash);
	return new_path;
}

/* Goes through each directory recursively and finds files to copy */
int recursiveCopy(char* dname){
	//travel through all directories and copy files
	// into the same directory structure
	int num_threads = 0;
	copy_args *root = (copy_args *) malloc(sizeof(copy_args));	
	if (root == NULL){
		perror("recursiveCopy");
	}
	copy_args *previous = root;
	int i = 0;

	struct dirent* ds;
	DIR* dir = opendir(dname);
	while((ds = readdir(dir)) != NULL){
		if(DEBUG2){
			printf("ds->d_name: %s\n",ds->d_name);
			printf("strncmp result \".\": %d\n", strncmp(ds->d_name, ".",1));
			printf("strncmp result \"..\": %d\n", strncmp(ds->d_name,"..",2));
			printf("%d\n",strlen(ds->d_name));
		}
		//while the next directory is not null
		if (strlen(ds->d_name) > 2 && strncmp(ds->d_name, ".backup", 7) != 0 || strncmp(ds->d_name, ".", 1) != 0 && strncmp(ds->d_name, "..", 2) != 0){
			//and this is not the current or previous directory structure
			//check status of object
			struct stat st;
			char fname[256] = "";
			strncat(fname, dname, strlen(dname));
			strcat(fname, "/");
			strncat(fname, ds->d_name, strlen(ds->d_name));

			//treating symlinks as symlinks, not the files they link to
			int err = lstat(fname, &st);
			if (err == -1){
				perror("recursiveCopy");
				return 1;
			}

			//check if this is a regular file
			if (S_ISREG(st.st_mode)){
				// init current node
				copy_args *current = (copy_args *) malloc(sizeof(copy_args));
				if (current == NULL){
					perror("recursiveCopy");
					freeArgs(root);
				}	
				current->next = NULL;

				//make a new filename for the copy
				char* path = removeParentDir(fname);
				char dest[4096] = BACKUP_DIR;
				strncat(dest, path, strlen(path));
				strcat(dest, ".bak");
				free(path);
				
				// store variables in struct to avoid sharing memory
				strncpy(current->filename, fname, strlen(fname) + 1);
				strncpy(current->destination, dest, strlen(dest) + 1);
				current->modifiedTime = st.st_mtime;
				current->threadNum = total_threads;
				total_threads++;
				i++;

				// connect linked list pointers
				previous->next = current;
				previous = current;
			}
			else if (S_ISDIR(st.st_mode)){
				if (DEBUG) printf("[  main  ] TODO: skipping directory '%s'\n", fname);
				char *newPath = removeParentDir(fname);
				char backup[4096] = BACKUP_DIR;
				strncat(backup, newPath, strlen(newPath));
				free(newPath);
				if (DEBUG) printf("Checking to see if %s exists... ", backup);
				int status = checkDir(backup);
				if (DEBUG) if (status == 2) printf("directory already exists!\n");
				else if (status == 0) printf("Created new directory %s\n", backup);
				recursiveCopy(fname);
			}
		}
	}
	closedir(dir);

	// run the threads, free the memory
	traverseCopyList(root, i);
	free(root);
}

// frees the copy_args linked list
void freeArgs(copy_args *root){
	copy_args *current = root->next;
	copy_args *free_me = NULL;
	while(current != NULL){
		free_me = current;
		current = current->next;
		free(free_me);
	}
}

/*
Traverses the linked list
Skips root as it expects root to only hold a pointer 
to the starting node
Frees the memory of all
*/
void traverseCopyList(copy_args *root, int count){
	copy_args *current;
	copy_args *free_me;
	current = root->next;
	pthread_t thread_list[count];
	int total = 0;
	// create threads
	while(current != NULL){
		if (pthread_create(&thread_list[total], NULL, backupThread, current)){
			perror("traverseCopyList");
			freeArgs(root);
			return;
		}
		total++;
		current= current->next;
	}
	int thread = 0;
	for (int i = 0; i < total; i++){
		pthread_join(thread_list[i], NULL);
		if (DEBUG) printf("[  main  ] Joined thread %d of %d\n", i, total-1);
	}
	freeArgs(root);
}

/* Goes though the restore_args linked list, creates threads based on
 how many files to restore and restores them */
void traverseRestoreList(restore_args *root, int count){
	restore_args *current;
	restore_args *free_me;
	current = root->next;
	pthread_t thread_list[count];
	int total = 0;
	// create threads
	while(current != NULL){
		if (pthread_create(&thread_list[total], NULL, restoreThread, current)){
			perror("traverseRestoreList");
			freeRestoreLinkedList(root);
			return;
		}
		total++;
		current = current->next;
	}
	int thread = 0;
	for (int i = 0; i < total; i++){
		pthread_join(thread_list[i], NULL);
		if (DEBUG) printf("[  main  ] Joined thread %d of %d\n", i, total-1);
	}
	freeRestoreLinkedList(root);
}

/*
Traverses the linked list
Skips root as it expects root to only hold a pointer 
to the starting node
*/
void printCopyLinkedList(copy_args *root){
	copy_args *current;
	current = root->next;
	int count = 0;
	while(current != NULL){
		printf("\t----\n");
		printf("\t%d\n", current->threadNum);
		printf("\t%d\n", current->modifiedTime);
		printf("\t%s\n", current->filename);
		printf("\t%s\n", current->destination);
		current = current->next;
	}
}

// Frees all nodes of the linked list except for the root node 
void freeRestoreLinkedList(restore_args *root){
	restore_args *current;
	restore_args *free_me;
	current = root->next;
	int count = 0;
	while(current != NULL){
		free_me = current;
		current = current->next;
		free(free_me);
	}
}

// Prints the linked list to restore
void printRestoreLinkedList(restore_args *root){
	printf("[  main  ] Printing Restore Linked List\n");
	restore_args *current;
	current = root->next;
	int count = 0;
	while(current != NULL){
		printf("\t----\n");
		printf("\tthreadNum: %d\n", current->threadNum);
		printf("\tdestination: %s\n", current->destination);
		current = current->next;
		count++;
	}
}

/* This method takes out the .backup directory and the .bak file extention
   In order to restore the file where it is originally was located. */
int backupToMainPath(char* result, char* dirName, char* fileName){
	//assume the file name has a .bak extension
	char dirName2 [PATH_MAX] = "";
	strncpy(dirName2, dirName, strlen(dirName));
	char *dname = dirName2;
	char *parent = strtok_r(dname,"/",&dname);
	strncpy(result,parent,strlen(parent));
	char *tmp;
	while((tmp = strtok_r(dname, "/",&dname)) != NULL){
		if (strncmp(tmp,".backup",7)== 0){
			continue;
		} else {
			strncat(result, "/", strlen(result));
			strncat(result,tmp,strlen(tmp));
		}
	}
	if (fileName != NULL){
		strncat(result,"/",strlen(result));
		strncat(result,fileName,strlen(fileName)-4);
	}
	return 0;
}

// Thread to take over copying a file from the .backup directory
void *restoreThread(void *arg){
	struct restore_args args = *(struct restore_args*)arg;
	char destination[256] = "";
	strncpy(destination,args.destination,strlen(args.destination));
	char *filename = NULL;
	char *tmp;
	char *rest = destination;
	// Grabs just the filename from the path
	while((tmp = strtok_r(rest,"/",&rest))){
		filename = tmp;
	}
	// Before copy
	printf("[thread %d] Restoring %s\n",args.threadNum, filename);
	// args.canCpy is determined in recursiveRestore.
	if(args.canCpy){
		int bytes = copyFile(args.fileToRestore, args.destination);
		// If successful, display message, else display error
		if (bytes != -1){
			printf("[thread %d] Copied %d bytes from %s.bak to %s\n", args.threadNum,bytes,
				filename,filename);
			updateTotalBytes(bytes);
		} else {
			printf("[thread %d] ERROR: could not copy %s.bak to %s\n", args.threadNum, filename,filename);
		}
	} else {
		printf("[thread %d] NOTICE: %s is already the most current version\n",args.threadNum, filename);
	}
	fclose(args.fileToRestore); 
}

/* recursively goes through each suBACKUP_DIRectory and file in the .backup directory 
   and counts the files that needs to be copied */
int recursiveRestore(char* dname){
	//similar to recursive copy, only moving files from the 
	// backup directory to the main directory
	int num_threads = 0;
	restore_args *root = (restore_args *) malloc(sizeof(restore_args));	
	if (root == NULL){
		perror("recursiveCopy");
	}
	strncpy(root->destination, "",1);
	root->threadNum = 0;
	root->fileToRestore = 0;
	root->next = NULL;
	root->canCpy = 0;
	restore_args *previous = root;
	int i = 0;
	DIR *backupDir = opendir(dname);
	struct dirent *backupDirent;
	while((backupDirent = readdir(backupDir)) != NULL){
		//check the current object
		if (strlen(backupDirent->d_name) > 2 && strncmp(backupDirent->d_name,".backup",7) != 0 || strncmp(backupDirent->d_name, ".", 1) != 0 && strncmp(backupDirent->d_name, "..", 2) != 0){
		
		
		//prepend the working directory to the file name
		char fname[256] = "";
		strcat(fname, dname);
		strcat(fname, "/");
		strncat(fname, backupDirent->d_name, strlen(backupDirent->d_name));

		struct stat backup;
		int err = lstat(fname, &backup);
		if (err == -1){
			perror("recursiveRestore");
			return 1;
		}

		//check if it is a regular file
		if (S_ISREG(backup.st_mode)){
			if (DEBUG){
				printf("[  main  ] Working on file %s\n", fname);
			}
			//copy the regular file to the main directory
			//check if the file already exists and/or is newer than the backup
			int canCopy = 1;
			char newDest[256] = "";
			if (access(fname, F_OK) != -1){
				//file exists
				struct stat tmp;
				if (DEBUG) printf("dname = %s\n", dname);
				if (DEBUG) printf("backupDirent->d_name = %s\n", backupDirent->d_name);
				backupToMainPath(newDest, dname, backupDirent->d_name);

				err = lstat(newDest, &tmp);
				if (err == -1){
					perror("recursiveRestore");
				} else{
					//compare modification times
					canCopy = (backup.st_mtime < tmp.st_mtime);
				}
			}
			if (DEBUG){
				printf("canCopy: %d\n", canCopy);
			}

			if (DEBUG){
				printf("[  main  ] Restoring file.\n");
			}
			//perform the copy
			// init current node
			restore_args *current = (restore_args *) malloc(sizeof(restore_args));
			if (current == NULL){
				perror("recursiveRestore");
				freeRestoreLinkedList(root);
			}	
			current->next = NULL;
			current->fileToRestore = NULL;
			strncpy(current->destination, "",1);
			current->threadNum = 0;
			current->canCpy = canCopy;

			//open file for reading only
			FILE *fp = fopen(fname, "r");
			if (fp == NULL){
				perror("recursiveRestore");
				return 1;
			}
			current->fileToRestore = fp;
			strncpy(current->destination, newDest, strlen(newDest) + 1);
			if (DEBUG){
				printf("[  main  ] newDest: %s\n", newDest);
				printf("[  main  ] current->destination: %s\n", current->destination);
			}
			total_threads++;
			i++;
			current->threadNum = total_threads;

			// connect linked list pointers
			previous->next = current;
			previous = current;

		} else if (S_ISDIR(backup.st_mode)){
			char splitString[4096] = "";
			strncpy(splitString,fname,strlen(fname));
			char *splitString2 = splitString;
			char fullDir [4096] = "";
			backupToMainPath(fullDir,fname,NULL);
			if (DEBUG) printf("fullDir = %s\n", fullDir);
			checkDir(fullDir);
			recursiveRestore(fname);
		}
		}
	}
	closedir(backupDir);
	if (DEBUG) printRestoreLinkedList(root);
	traverseRestoreList(root, i);
	free(root);
}

// recursively traverse the directory and counts the number of files
int countFiles(char* dname){
	int count = 0;
	//travel through all directories and copy files
	// into the same directory structure
	struct dirent* ds;
	DIR* dir = opendir(dname);
	while((ds = readdir(dir)) != NULL){
		//while the next directory is not null
		if (strncmp(ds->d_name, ".", 1) != 0 && strncmp(ds->d_name, "..", 2) != 0){
			//and this is not the current or previous directory structure
			//check status of object
			struct stat st;
			
			char fname[256] = "";
			strncat(fname, dname, strlen(fname) + strlen(dname) + 1);
			strncat(fname, "/", strlen(fname) + 2);
			strncat(fname, ds->d_name, strlen(fname) + strlen(ds->d_name) + 1);

			//treating symlinks as symlinks, not the files they link to
			int err = lstat(fname, &st);
			if (err == -1){
				perror("countFiles");
				return 1;
			}
			//check if this is a regular file
			if (S_ISREG(st.st_mode)){
				count++;
			}
			else if (S_ISDIR(st.st_mode)){
				count += countFiles(fname);
			}
		}
	}
	closedir(dir);
	return count;
}

// JOIN threads list
void joinThreads(pthread_t thread_list[], int count){
	alarm(1);
	pause();
	for (int i = 0; i < count; i++){
		if (DEBUG) printf("[thread %d] waiting to join\n", i);
		pthread_join(thread_list[i], NULL);
		if (DEBUG) printf("[thread %d] has joined\n", i);
	}
}

// main function of the program.
int main(int argc, char **argv){
	char * backupDirectory = ".";
	char * restoreDirectory = BACKUP_DIR;
	if (DEBUG) printf("[thread main] CWD: %s\n", backupDirectory);
	int restore = 0;
	for(int i = 0; i < argc; i++){
		if (strncmp(argv[i], "-r", 2) == 0){
			restore = 1;
			break;
		}
	}
	if (restore){
		if (DEBUG){
			printf("[  main  ] Restoring from backup.\n");
		}
		// Look for the .backup directory
		DIR *backup = opendir(restoreDirectory);
		if(ENOENT == errno){
			printf("./BackItUp: No .backup directory found.\n");
			return 1;
		}
		closedir(backup);
		if(recursiveRestore(restoreDirectory)==1){
			return 1;
		}
	} else{
		if (createBackupDir()){
			return 1;
		}
		if (recursiveCopy(backupDirectory)){
			return 1;
		}
	}
	pthread_mutex_lock(&lock);
	printf("Successfully copied %d files (%d bytes)\n", successfulFiles, totalBytes);
	pthread_mutex_unlock(&lock);
	pthread_mutex_destroy(&lock);

	return 0;
}
