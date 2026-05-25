#include "common.h"
#include "qsocket.h"
#include "qfile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

static int sendCfg2Monitor();
static int startResvThread();
static int startResvThread_p();

int initMainSocket()
{
    g_server_fd = 0;
    struct sockaddr_in server_addr;

    // 创建套接字
    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0)
    {
        QLOGD("Socket creation failed\n");
        return EXIT_FAILURE;
    }

    // 设置服务器地址结构
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(g_qparameter.monitor_port);
    inet_pton(AF_INET, g_qparameter.monitor_ip, &server_addr.sin_addr);

    // 连接到服务器
    if (connect(g_server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        QLOGD("Connection failed\n");
        close(g_server_fd);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int closeMainSocket()
{
    if (g_server_fd < 0) return 0;
    close(g_server_fd);
    g_server_fd = -1;
    return 0;
}

int parserMonitorMes(const char *str)
{
    QLOGD("[%s][%d] entry [%s]\n", __func__, __LINE__, str);
    if (str == NULL)
    {
        QLOGD("str is null pls check");
        return -1;
    }
    char *mes = strdup(str);

    char *token = strtok(mes, "-");
    int num = 0;
    while (token != NULL)
    {
        if (num == 0)
        {
            strcpy(g_qparameter.monitor_ip, token);
        }
        else
        {
            g_qparameter.monitor_port = atoi(token);
        }
        num++;
        token = strtok(NULL, "-");
    }

    free(mes);

    return 0;
}

int handleMessage(const char *buff)
{
    //
    // if (strstr(buff, "filter") != NULL)
    if (buff[0] == 101)
    {
        QLOGD("message type 1\n");
        sendCfg2Monitor();
    }
    // else if (strstr(buff, "start") != NULL)
    if (buff[0] == 102)
    {
        QLOGD("message type 2\n");

#ifdef USING_PTHREAD
        startResvThread_p();
#else
        startResvThread();
#endif
    }
    else
    {
        QLOGD("error message type do not process\n");
    }
    return 0;
}

static int sendCfg2Monitor()
{
    //
    char buffer[1 * 1024 + 5];
    int length = 0;
    memset(buffer, 0, sizeof(buffer));
    buffer[0] = 3;
    memcpy(buffer + 1, &length, 4);
    send(g_server_fd, buffer, 5, 0);
    sleep(2);

    memset(buffer, 0, sizeof(buffer));
    buffer[0] = 4;
    int fd = qreadFile(g_qparameter.cfg_file_path);
    int bytes_read;

    while ((bytes_read = read(fd, buffer + 5, sizeof(buffer) - 5)) > 0)
    {
        memcpy(buffer + 1, &bytes_read, 4);
        send(g_server_fd, buffer, bytes_read + 5, 0);
        usleep(200 * 1000);
    }
    qcloseFile(fd);

    sleep(2);
    memset(buffer, 0, sizeof(buffer));
    buffer[0] = 5;
    memcpy(buffer + 1, &length, 4);
    send(g_server_fd, buffer, 5, 0);
    return 0;
}

static void startServer(int type);
void *startServer_p(void *num);

static int startResvThread_p()
{
    int rc;
    long t;
    for (t = 0; t < g_qparameter.log_type; t++)
    {
        QLOGD("Creating thread %ld\n", t);
        rc = pthread_create(&g_threads[t], NULL, startServer_p, (void *)t);

        if (rc)
        {
            QLOGD("Error: unable to create thread %ld, return code: %d\n", t, rc);
            exit(-1);
        }
    }
    return 0;
}

static int startResvThread()
{
    switch (g_qparameter.log_type)
    {
        case 1:
            QLOGD("will start qxdm thread to resv data\n");
            g_diag_child_pid = fork();
            if (g_diag_child_pid < 0)
            {
                QLOGD("[%s][%d] fork error\n", __func__, __LINE__);
            }
            else if (g_diag_child_pid == 0)
            {
                startServer(1);
            }
            break;
        case 2:
            QLOGD("will start qxdm and qdss thread to resv data\n");
            for (int i = 0; i < 2; i++)
            {
                if (i == 0)
                {
                    g_diag_child_pid = fork();
                    if (g_diag_child_pid < 0)
                    {
                        QLOGD("[%s][%d] fork error\n", __func__, __LINE__);
                    }
                    else if (g_diag_child_pid == 0)
                    {
                        startServer(1);
                    }
                }
                else if (i == 1)
                {
                    g_qdss_child_pid = fork();
                    if (g_qdss_child_pid < 0)
                    {
                        QLOGD("[%s][%d] fork error\n", __func__, __LINE__);
                    }
                    else if (g_qdss_child_pid == 0)
                    {
                        startServer(2);
                    }
                }
                else
                    QLOGD("error num!");
            }
        case 3:
            QLOGD("will start qxdm qdss and adpl thread to resv data\n");
            for (int i = 0; i < 3; i++)
            {
                if (i == 0)
                {
                    g_diag_child_pid = fork();
                    if (g_diag_child_pid < 0)
                    {
                        QLOGD("[%s][%d] fork error\n", __func__, __LINE__);
                    }
                    else if (g_diag_child_pid == 0)
                    {
                        startServer(1);
                    }
                }
                else if (i == 1)
                {
                    g_qdss_child_pid = fork();
                    if (g_qdss_child_pid < 0)
                    {
                        QLOGD("[%s][%d] fork error\n", __func__, __LINE__);
                    }
                    else if (g_qdss_child_pid == 0)
                    {
                        startServer(2);
                    }
                }
                else if (i == 2)
                {
                    g_adpl_child_pid = fork();
                    if (g_adpl_child_pid < 0)
                    {
                        QLOGD("[%s][%d] fork error\n", __func__, __LINE__);
                    }
                    else if (g_adpl_child_pid == 0)
                    {
                        startServer(3);
                    }
                }
                else
                    QLOGD("error num!");
            }
            break;

        default: QLOGD("will start qxdm thread to resv data\n"); break;
    }

    return 0;
}

void *startServer_p(void *num)
{
    long type_t = (long)num;
    int type = (int)type_t + 1;
    QLOGD("[%s][%d] entry! typr:[%d] pid:[%d]\n", __func__, __LINE__, type, getpid());
    int server_fd, client_fd;
    uint16_t port = 0;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    int opt = 1;

    // 创建套接字
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        QLOGD("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 设置 socket 选项以允许重用地址
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        QLOGD("setsockopt");
        exit(EXIT_FAILURE);
    }

    // 设置服务器地址结构
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (type == 1) port = MDLOG_DM_SOCKET_PORT;
    if (type == 2) port = MDLOG_QDSS_SOCKET_PORT;
    if (type == 3) port = MDLOG_ADPL_SOCKET_PORT;
    server_addr.sin_port = htons(port);

    // 绑定
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        QLOGD("Bind failed\n");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 监听
    listen(server_fd, 3);
    QLOGD("Server listening on port %d\n", port);

    initFile();

    // 打开文件
    while (!g_request_exit)
    {
        // 接受客户端连接
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0)
        {
            QLOGD("Accept failed");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        QLOGD("Accepted connection from %s:%d\n", client_ip, ntohs(client_addr.sin_port));

        // 接收数据
        ssize_t bytes_received;
        while ((bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0)
        {
            buffer[bytes_received] = '\0';                           // 添加字符串结束符
            int res = qwriteData2File(type, buffer, bytes_received); // 写入文件
            if (res < 0)
            {
                QLOGD("qwirte data to file return error!");
            }

            if (g_request_exit > 0)
            {
                break;
            }
        }

        close(client_fd);
        usleep(500 * 1000);
    }

    close(server_fd);

    QLOGD("[%s] exit! type: [%d]\n", __func__, type);
    pthread_exit(NULL); // 退出线程
    // return;
}

static void startServer(int type)
{
    QLOGD("[%s][%d] entry! typr:[%d] pid:[%d]\n", __func__, __LINE__, type, getpid());
    int server_fd, client_fd;
    uint16_t port = 0;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // 创建套接字
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        QLOGD("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 设置服务器地址结构
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (type == 1) port = MDLOG_DM_SOCKET_PORT;
    if (type == 2) port = MDLOG_QDSS_SOCKET_PORT;
    if (type == 3) port = MDLOG_ADPL_SOCKET_PORT;
    server_addr.sin_port = htons(port);

    // 绑定
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        QLOGD("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 监听
    listen(server_fd, 3);
    QLOGD("Server listening on port %d\n", port);

    // 打开文件

    while (!g_request_exit)
    {
        // 接受客户端连接
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0)
        {
            QLOGD("Accept failed");
            continue;
        }

        // 接收数据
        ssize_t bytes_received;
        while ((bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0)
        {
            buffer[bytes_received] = '\0';                           // 添加字符串结束符
            int res = qwriteData2File(type, buffer, bytes_received); // 写入文件
            if (res < 0)
            {
                QLOGD("qwirte data to file return error!");
            }

            if (g_request_exit > 0)
            {
                break;
            }
        }

        close(client_fd);
        usleep(500 * 1000);
    }

    close(server_fd);

    QLOGD("[%s] exit! type: [%d]\n", __func__, type);
}

int handleMD5(const char *filename, char *result, int *size)
{
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "md5sum %s", filename); // 构造命令

    // 使用 popen 调用 md5sum 命令
    FILE *fp = popen(command, "r");
    if (fp == NULL)
    {
        QLOGD("popen failed");
        return -1;
    }

    if (fgets(result, 33, fp) == NULL)
    {
        QLOGD("Failed to read the output from md5sum\n");
    }

    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        printf("File not found: %s\n", filename);
        return -1;
    }

    // 计算文件大小
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    QLOGD("[%s][%d] md5: [%s] file size:[%u]\n", __func__, __LINE__, result, *size);

    fclose(file);

    // 关闭管道
    if (pclose(fp) == -1)
    {
        QLOGD("pclose failed");
        return -1;
    }

    return 0;
}
