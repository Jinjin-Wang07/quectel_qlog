#ifndef __QSERVER_H__
#define __QSERVER_H__

#include <stdio.h>
#include <stdint.h>

struct QServer
{
    uint32_t dm_server_port;
    uint32_t qdss_server_port;
    uint32_t adpl_server_port;
    char *file_path;
    uint32_t file_size;
    uint32_t file_num;
};

#define DM_TYPE 0
#define QDSS_TYPE 1
#define ADPL_TYPE 2

extern struct QServer g_qserver;

int qSocketServerHandle();
void parserServerPort(const char *opt);

#endif //__QSERVER_H__