#include <stdio.h>
#ifndef BACKITUP_H
#define BACKITUP_H

struct thread_args {
    time_t modifiedTime;
    char filename[256];
    char destination[256];
    int threadNum;
};

struct restore_args {
    FILE *fileToRestore;
    char destination[256];
    int threadNum;

};
void printError(char* error);

int createBackupDir();

int copyFile(FILE* fp, char* fname);

int recursiveCopy(char* dname);

void * createBackupFile(void *argument);

void joinThreads(pthread_t *thread_list, int count);

int countFiles(char* dname);

#endif