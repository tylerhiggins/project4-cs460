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

void * backupThread(void *argument);

void *restoreThread(void *arg);

void joinThreads(pthread_t *thread_list, int count);

int countFiles(char* dname);

void printLinkedList(copy_args *root);

void traverseList(copy_args *root, int count, char* method);

#endif
