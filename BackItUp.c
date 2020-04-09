#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/stat.h>

#include "BackItUp.h"

#define DEBUG 0

void printError(char* error){
	if( DEBUG ){
		printf("Error: %s\n", error);
	}
}

int createBackupDir(){
	int err = mkdir("testdir/.backup", 0777);
	if( err == -1 ){
		if( errno == EEXIST ){
			printf("Directory already exists.\n");
			return 0;
		}else{
			printError(strerror(errno));
		}
	}
	return 0;
}


int main(int argc, char **argv) {

	createBackupDir();

	return 0;
}



