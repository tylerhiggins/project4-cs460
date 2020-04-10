
#ifndef BACKITUP_H
#define BACKITUP_H

void printError(char* error);

int createBackupDir();

int copyFile(FILE* fp, char* fname);

int recursiveCopy(char* dname);

#endif