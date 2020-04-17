
#ifndef BACKITUP_H
#define BACKITUP_H

struct thread_args {
    int threadNumber;
    time_t modifiedTime;
    char filename[256];
    char destination[256];
};

void printError(char* error);

int createBackupDir();

int copyFile(FILE* fp, char* fname);

int recursiveCopy(char* dname);

void * createBackupFile(void *argument);

#endif