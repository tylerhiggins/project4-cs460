
#ifndef BACKITUP_H
#define BACKITUP_H

void printError(char* error);

int createBackupDir();

int copyFile(FILE* fp, char* fname);

int recursiveCopy(char* dname);

struct thread_args {
    time_t modifiedTime;
    char filename[256];
    char destination[256];
};

#endif