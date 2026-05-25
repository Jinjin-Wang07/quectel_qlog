#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

typedef struct
{
    int id;
    pthread_mutex_t *lock;
    char directory[256];
} ThreadData;

void *file_writer(void *arg);
void remove_oldest_file(const char *directory);
int qreadFile(const char *filename);
void qcloseFile(int fd);
int qwriteData2File(int type, const char *data, int size);
void initFile();

#endif // FILE_MANAGER_H
