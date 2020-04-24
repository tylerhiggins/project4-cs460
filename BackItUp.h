#include <stdio.h>

#ifndef BACKITUP_H
#define BACKITUP_H

#define PATH_MAX 4096
#define NAME_MAX 255

typedef struct copy_args node;
typedef struct restore_args restore_node;

typedef struct copy_args {
    int root;
    time_t modifiedTime;
    char filename[PATH_MAX];
    char destination[PATH_MAX];
    int threadNum;
    node *next;
} copy_args;

typedef struct restore_args {
    FILE *fileToRestore;
    char destination[PATH_MAX];
    int threadNum;
    restore_node *next;
} restore_args;

void printError(char* error);

int createBackupDir();

int copyFile(FILE* fp, char* fname);

int recursiveCopy(char* dname);

void * backupThread(void *argument);

void *restoreThread(void *arg);

void joinThreads(pthread_t *thread_list, int count);

int countFiles(char* dname);

void printLinkedList(copy_args *root);

void freeArgs(copy_args *root);

void traverseCopyList(copy_args *root, int count);

void traverseRestoreList(restore_args *root, int count);

void freeRestoreLinkedList(restore_args *root);

#endif
