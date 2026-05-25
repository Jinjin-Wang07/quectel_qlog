#ifndef __COMMOM_H__
#define __COMMOM_H__

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>

#define USING_PTHREAD // 1 // 1- 多线程模式 2 - 代表多进程模式

#define QSOCKET_LOGFILE_NUM 512
#define QSOCKET_LOGFILE_SIZE 256 // MB
#define BUFFER_SIZE 1024 * 1024
#define MAX_CONNECTIONS 10
#define MAX_FILE_SIZE (100 * 1024 * 1024) // 100MB

#define NUM_SERVERS 3

#define MDLOG_MONITOR_SOCKET_PORT 14000
#define MDLOG_DM_SOCKET_PORT 14001
#define MDLOG_QDSS_SOCKET_PORT 14002
#define MDLOG_ADPL_SOCKET_PORT 14003

unsigned qsocketCommonMsecs(void);

#define QLOGD(fmt, arg...)                                                 \
    do                                                                     \
    {                                                                      \
        unsigned msec = qsocketCommonMsecs();                              \
        printf("[INFO][%03u:%03u]:" fmt, msec / 1000, msec % 1000, ##arg); \
        fflush(stdout);                                                    \
    } while (0)

struct QParameter
{
    int socket_clent_action_time;
    int one_log_file_max_size; // MB
    int number_file;
    char cfg_file_path[256];
    int log_type;
    char monitor_ip[256];
    unsigned int monitor_port;
    char workspace_path[256];
};

extern struct QParameter g_qparameter;
extern int g_server_fd;
extern int g_request_exit;
extern pid_t g_diag_child_pid;
extern pid_t g_qdss_child_pid;
extern pid_t g_adpl_child_pid;
extern pthread_t g_threads[NUM_SERVERS];

void initParameter();
int parserArgv(int argc, char **argv);
int parserMonitorMes(const char *str);
int initMainSocket();
int closeMainSocket();
int handleMessage(const char *buff);
int handleMD5(const char *filename, char *result, int *size);
#endif //__COMMOM_H__