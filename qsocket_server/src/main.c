#include <stdio.h>
#include "common.h"
#include "qsocket.h"
#include <error.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <pthread.h>

struct QParameter g_qparameter;
int g_server_fd;
int g_request_exit;
pid_t g_diag_child_pid;
pid_t g_qdss_child_pid;
pid_t g_adpl_child_pid;
pthread_t g_threads[NUM_SERVERS]; // 创建线程的数组

static void qserverFun(int sig)
{
    if (sig == SIGINT) g_request_exit = 1;
    QLOGD("Resv sigal [%d] pid:[%d]\n", sig, getpid());
    return;
}

//
// 1. 连接后发送” 1 4bytes 4bytes 32bytes“，分别代表filter的大小，日志类型和filter的md5值。前面加上一个字节，内容为数字1
// 2. ”2 stop“，停止抓日志，前面加一个字节，内容为数字2，甚至后面的stop都可以不要了。因为我只要判断是2，就可以结束diag_mdlog
// 3. 如果我请求filter了，你给我发 ”3 filter_start“，表示下面要发filter内容了，我这边可以打开做创建和打开文件操作
// 4  你给我发filter的内容，前面加一个字节，内容为数字4，是每次发1kb的filter前面都要加一个4，相当于每次发1024+1个字节，我直接写第三步打开的文件
// 5. filter发送完成后，你给我发一个” 5filter_end“，我就可以关闭文件。然后这样我保存文件的md5和大小和你第一步发的是否一致
//

static void sendHueryUpMessage(const char *md5_result, const int file_size);
static void sendStopMessage();

int main(int argc, char *argv[])
{
    QLOGD("VERSION:QLOG_QSOCKETSERVER_Linux_V1.0\n");

    char buffer[BUFFER_SIZE];
    g_request_exit = 0;
    if (signal(SIGINT, qserverFun) == SIG_ERR)
    {
        QLOGD("Unable to catch SIGINT");
        return -1;
    }
    QLOGD("[%s][%d] Running... Press Ctrl+C to stop.\n", __func__, __LINE__);

    char md5_result[33];
    int file_size = 0;

    initParameter();
    if (parserArgv(argc, argv) < 0) return -1;
    memset(md5_result, 0, sizeof(md5_result));
    handleMD5(g_qparameter.cfg_file_path, md5_result, &file_size);

    initMainSocket();

    // 发送消息
    sendHueryUpMessage(md5_result, file_size);

    // 接收响应
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(g_server_fd, &read_fds);

    while (!g_request_exit)
    {
        QLOGD("select main socket fd ... \n");
        int ret = select(g_server_fd + 1, &read_fds, NULL, NULL, 0);
        if (ret < 0)
        {
            QLOGD("select failed, error:%s\n", strerror(errno));
        }
        else if (ret == 0)
        {
            QLOGD("Timeout occurred! No data received.\n");
        }
        else
        {
            // 套接字可读，调用 recv()
            ssize_t bytes_received = recv(g_server_fd, buffer, BUFFER_SIZE - 1, 0);
            if (bytes_received < 0)
            {
                QLOGD("Receive failed\n");
            }

            buffer[bytes_received] = '\0'; // 添加字符串结束符
            QLOGD("Received from server: %s\n", buffer);
            handleMessage(buffer);
        }
        usleep(500 * 1000);
    }

    sendStopMessage();

    // 关闭套接字
    closeMainSocket();

    for (int i = 0; i < g_qparameter.log_type; i++)
    {
        if (pthread_join(g_threads[i], NULL) != 0)
        {
            QLOGD("Failed to join thread\n");
            exit(EXIT_FAILURE);
        }
    }

    // for (int i = 0; i < g_qparameter.log_type; i++)
    // {
    //     wait(NULL); // 等待任意子进程
    //     QLOGD("TYPE [%d] MAX [%d] exit successful\n", i, g_qparameter.log_type);
    // }
    QLOGD("All threads have finished. Exiting main thread.\n\n");
    return 0;
}

static void sendHueryUpMessage(const char *md5_result, const int file_size)
{
    //{[1 byte type id][4 bytes file size][4 bytes log type][32 bytes cfg file md5 value]}
    char send_mes_buf[4096];
    int length = 40;
    memset(send_mes_buf, 0, sizeof(send_mes_buf));
    send_mes_buf[0] = 1;                                      // protocal id
    memcpy(send_mes_buf + 1, &length, 4);                     // length
    memcpy(send_mes_buf + 5, &file_size, 4);                  // filter file size
    memcpy(send_mes_buf + 9, &g_qparameter.log_type, 4);      // qxdm log type
    snprintf(send_mes_buf + 13, 4096 - 13, "%s", md5_result); // filter file md5 value

    send(g_server_fd, send_mes_buf, 45, 0);
    return;
}

static void sendStopMessage()
{
    char send_mes_buf[4096];
    int length = 0;
    memset(send_mes_buf, 0, sizeof(send_mes_buf));
    send_mes_buf[0] = 2;
    memcpy(send_mes_buf + 1, &length, 4);
    length = send(g_server_fd, send_mes_buf, 5, 0);
    QLOGD("Send close: length is %d", length);
    return;
}
