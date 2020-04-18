#include <stdio.h>

#ifndef BACKITUP_H
#define BACKITUP_H

#define PATH_MAX 4096
#define NAME_MAX 255

typedef struct copy_args node;

typedef struct copy_args {
    int root;
    time_t modifiedTime;
    char filename[PATH_MAX];
    char destination[PATH_MAX];
    int threadNum;
    node *next;
} copy_args;

struct restore_args {
    FILE *fileToRestore;
    char destination[PATH_MAX];
    int threadNum;

};
void printError(char* error);

int createBackupDir();

int copyFile(FILE* fp, char* fname);

int recursiveCopy(char* dname);

void * createBackupFile(void *argument);

void joinThreads(pthread_t *thread_list, int count);

int countFiles(char* dname);

copy_args** initStructList();

void doubleListSize(copy_args* struct_list);

void printStructList(copy_args **struct_list, int count);

void traverseList(copy_args *root);

#endif
