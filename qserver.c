#include "qlog.h"
#include "qserver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <pthread.h>

#define server_port_spilt_str "-"
void parserServerPort(const char *opt)
{
    char *str = strdup(opt);
    char *token;
    int check_int = 0;
    token = strtok(str, server_port_spilt_str);
    while (token != NULL)
    {
        qlog_dbg("%s %d\n", token, check_int);
        if (check_int == 0)
            g_qserver.dm_server_port = strtol(token, NULL, 10);
        else if (check_int == 1)
            g_qserver.qdss_server_port = strtol(token, NULL, 10);
        else if (check_int == 2)
            g_qserver.adpl_server_port = strtol(token, NULL, 10);
        else
        {
            qlog_dbg("too much port to input!\r\n");
            break;
        }
        token = strtok(NULL, server_port_spilt_str);
        check_int += 1;
    }
    qlog_dbg("[%s][%d] dm:[%d] qdss:[%d] adpl:[%d]\r\n", __func__, __LINE__, g_qserver.dm_server_port, g_qserver.qdss_server_port, g_qserver.adpl_server_port);
    free(str);
    return;
}

static int file_fds[3] = {0};
static pthread_t dm_tid;
static pthread_t qdss_tid;
static pthread_t adpl_tid;

void initFile(int flag);
void initSocket(int flag);

int qSocketServerHandle()
{
    qlog_dbg("[%s][%d] entry!\r\n", __func__, __LINE__);
    if (g_qserver.dm_server_port > 0)
    {
        initFile(0);
        initSocket(0);
    }
    if (g_qserver.qdss_server_port > 0)
    {
        initFile(1);
        initSocket(1);
    }
    if (g_qserver.adpl_server_port > 0)
    {
        initFile(2);
        initSocket(2);
    }

    while (!qlog_exit_requested)
    {
        sleep(10);
        qlog_dbg("in cycle and run!\r\n");
    }

    if (g_qserver.dm_server_port > 0)
    {
#ifdef USE_NDK
        // TODO Android NDK do not support pthread_cancel
#else
        pthread_cancel(dm_tid);
#endif
    }
    if (g_qserver.qdss_server_port > 0)
    {
#ifdef USE_NDK
        // TODO Android NDK do not support pthread_cancel
#else
        pthread_cancel(qdss_tid);
#endif
    }
    if (g_qserver.adpl_server_port > 0)
    {
#ifdef USE_NDK
        // TODO Android NDK do not support pthread_cancel
#else
        pthread_cancel(adpl_tid);
#endif
    }

    close(file_fds[0]);
    close(file_fds[1]);
    close(file_fds[2]);
    return 0;
}

void initFile(int flag)
{
    char shortname[128];
    char filename[256];
    switch (flag)
    {
        case 0:
            memset(shortname, 0, sizeof(shortname));
            memset(filename, 0, sizeof(filename));

            snprintf(shortname, sizeof(shortname), "%.80s", qlog_time_name(1));
            if (g_qserver.file_path[0] == '\0')
                sprintf(filename, "%s/%s.qmdl", g_qserver.file_path, shortname);
            else
                sprintf(filename, "./%s.qmdl", shortname);

            qlog_dbg("dm save file name:[%s]\r\n", filename);

            file_fds[0] = open(filename, O_CREAT | O_RDWR | O_TRUNC, 0444);
            break;

        case 1:
            memset(shortname, 0, sizeof(shortname));
            memset(filename, 0, sizeof(filename));

            snprintf(shortname, sizeof(shortname), "%.80s", qlog_time_name(1));
            if (g_qserver.file_path[0] == '\0')
                sprintf(filename, "%s/%s_qdss.bin", g_qserver.file_path, shortname);
            else
                sprintf(filename, "./%s_qdss.bin", shortname);

            qlog_dbg("qdss save file name:[%s]\r\n", filename);

            file_fds[1] = open(filename, O_CREAT | O_RDWR | O_TRUNC, 0444);
            break;

        case 2:
            memset(shortname, 0, sizeof(shortname));
            memset(filename, 0, sizeof(filename));

            snprintf(shortname, sizeof(shortname), "%.80s", qlog_time_name(1));
            if (g_qserver.file_path[0] == '\0')
                sprintf(filename, "%s/%s.adplv", g_qserver.file_path, shortname);
            else
                sprintf(filename, "./%s.adplv", shortname);

            qlog_dbg("adpl save file name:[%s]\r\n", filename);

            file_fds[2] = open(filename, O_CREAT | O_RDWR | O_TRUNC, 0444);
            break;

        default: qlog_dbg("not have type 3,pls check it!"); break;
    }
    return;
}

int qWrite2File(const char *buf, int size, int flag);

#define BUFFER_SIZE 1024
void *server_thread(void *arg)
{
    int type = *((int *)arg);
    int port = 0;
    port = type == 0 ? g_qserver.dm_server_port : port;
    port = type == 1 ? g_qserver.qdss_server_port : port;
    port = type == 2 ? g_qserver.adpl_server_port : port;
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // 创建套接字
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 设置套接字选项
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 配置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // 绑定套接字
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 监听连接
    if (listen(server_fd, 3) < 0)
    {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    qlog_dbg("Server is listening on port %d...\n", port);

    // 接受连接并处理数据
    while (!qlog_exit_requested)
    {
        // 接受客户端连接
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd < 0)
        {
            perror("accept failed");
            continue;
        }

        qlog_dbg("Client connected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // 读取客户端数据
        while ((bytes_read = read(client_fd, buffer, sizeof(buffer) - 1)) > 0)
        {
            buffer[bytes_read] = '\0'; // 确保字符串终止
            qlog_dbg("Received: %s\n", buffer);

            if (qWrite2File(buffer, bytes_read, type) < 0)
            {
                qlog_dbg("write file error: %ld\n", bytes_read);
                break;
            }
        }

        if (bytes_read < 0)
        {
            perror("read failed");
        }

        // 关闭客户端连接
        close(client_fd);
        sleep(2);
    }

    // 关闭服务器套接字
    close(server_fd);
    return 0;
}

void initSocket(int flag)
{
    int arg = -1;
    switch (flag)
    {
        case 0:
            arg = 0;
            if (pthread_create(&dm_tid, NULL, server_thread, &arg))
            {
                perror("pthread_create failed");
                exit(EXIT_FAILURE);
            }
            pthread_detach(dm_tid);
            break;
        case 1:
            arg = 1;
            if (pthread_create(&qdss_tid, NULL, server_thread, &arg))
            {
                perror("pthread_create failed");
                exit(EXIT_FAILURE);
            }
            pthread_detach(qdss_tid);
            break;
        case 2:
            arg = 2;
            if (pthread_create(&adpl_tid, NULL, server_thread, &arg))
            {
                perror("pthread_create failed");
                exit(EXIT_FAILURE);
            }
            pthread_detach(adpl_tid);
            break;
        default: break;
    }
    return;
}

static size_t dm_file_num = 0;
static size_t qdss_file_num = 0;
static size_t adpl_file_num = 0;
static size_t dm_data_len = 0;
static size_t qdss_data_len = 0;
static size_t adpl_data_len = 0;

int qWrite2File(const char *buf, int size, int flag)
{
    int ret = 0;
    switch (flag)
    {
        case 0:
            ret = write(file_fds[0], buf, size);
            if (ret < 0)
            {
                qlog_dbg("file write error!");
                return ret;
            }
            dm_data_len += ret;
            // if (dm_file_num > g_qserver.file_num)
            // {
            //     qlog_exit_requested = 1;
            //     return;
            // }
            if (dm_data_len > g_qserver.file_size)
            {
                close(file_fds[0]);
                file_fds[0] = -1;
                initFile(0);
                dm_file_num += 1;
            }
            break;
        case 1:
            ret = write(file_fds[1], buf, sizeof(buf));
            if (ret < 0)
            {
                qlog_dbg("file write error!");
                return ret;
            }
            qdss_data_len += ret;
            // if (qdss_file_num > g_qserver.file_num)
            // {
            //     qlog_exit_requested = 1;
            //     return -1;
            // }
            if (qdss_data_len > g_qserver.file_size)
            {
                close(file_fds[1]);
                file_fds[1] = -1;
                initFile(1);
                qdss_file_num += 1;
            }
            break;
        case 2:
            ret = write(file_fds[2], buf, sizeof(buf));
            if (ret < 0)
            {
                qlog_dbg("file write error!");
                return ret;
            }
            adpl_data_len += ret;
            // if (adpl_file_num > g_qserver.file_num)
            // {
            //     qlog_exit_requested = 1;
            //     return -1;
            // }
            if (adpl_data_len > g_qserver.file_size)
            {
                close(file_fds[2]);
                file_fds[2] = -1;
                initFile(2);
                adpl_file_num += 1;
            }
            break;
        default: break;
    }
    return 1;
}

/*
void handle()
{
    int server_fd1 = -1;
    int server_fd2 = -1;
    int server_fd3 = -1;
    int new_socket1, new_socket2, new_socket3;
    struct sockaddr_in address1, address2, address3;
    fd_set readfds;
    int max_sd, activity;
    char buffer[BUFFER_SIZE];
    int addrlen = sizeof(address1);

    if (g_qserver.dm_server_port > 0)
    {
        server_fd1 = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd1 == 0)
        {
            qlog_dbg("DM socket create failed!");
            return;
        }
        address1.sin_family = AF_INET;
        address1.sin_addr.s_addr = INADDR_ANY;
        address1.sin_port = htons(g_qserver.dm_server_port);
        if (bind(server_fd1, (struct sockaddr *)&address1, sizeof(address1)) < 0)
        {
            qlog_dbg("DM socket bind failed!");
            return;
        }
        if (listen(server_fd1, 3) < 0)
        {
            qlog_dbg("DM socket listen failed!");
            return;
        }
    }

    if (g_qserver.qdss_server_port > 0)
    {
        server_fd2 = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd2 == 0)
        {
            qlog_dbg("QDSS socket create failed!");
            return;
        }
        address2.sin_family = AF_INET;
        address2.sin_addr.s_addr = INADDR_ANY;
        address2.sin_port = htons(g_qserver.qdss_server_port);
        if (bind(server_fd2, (struct sockaddr *)&address2, sizeof(address2)) < 0)
        {
            qlog_dbg("QDSS socket bind failed!");
            return;
        }
        if (listen(server_fd2, 3) < 0)
        {
            qlog_dbg("QDSS socket listen failed!");
            return;
        }
    }

    if (g_qserver.adpl_server_port > 0)
    {
        server_fd3 = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd3 == 0)
        {
            qlog_dbg("ADPL socket create failed!");
            return;
        }
        address3.sin_family = AF_INET;
        address3.sin_addr.s_addr = INADDR_ANY;
        address3.sin_port = htons(g_qserver.adpl_server_port);
        if (bind(server_fd3, (struct sockaddr *)&address3, sizeof(address3)) < 0)
        {
            qlog_dbg("ADPL socket bind failed!");
            return;
        }

        if (listen(server_fd3, 3) < 0)
        {
            qlog_dbg("ADPL socket listen failed!");
            return;
        }
    }

    int valread;

    FD_ZERO(&readfds);
    max_sd = -1;
    if (server_fd1 > 0)
    {
        FD_SET(server_fd1, &readfds);
        max_sd = server_fd1;
    }
    if (server_fd2 > 0)
    {
        FD_SET(server_fd2, &readfds);
        max_sd = max_sd > server_fd2 ? max_sd : server_fd2;
    }
    if (server_fd3 > 0)
    {
        FD_SET(server_fd3, &readfds);
        max_sd = max_sd > server_fd3 ? max_sd : server_fd3;
    }

    // 主循环
    while (!qlog_exit_requested)
    {
        // 等待活动文件描述符
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0)
        {
            qlog_dbg("select error");
            exit(EXIT_FAILURE);
        }

        // 检查每个文件描述符
        valread = -1;
        if (FD_ISSET(server_fd1, &readfds))
        {
            if ((new_socket1 = accept(server_fd1, (struct sockaddr *)&address1, (socklen_t *)&addrlen)) < 0)
            {
                qlog_dbg("accept failed");
                exit(EXIT_FAILURE);
            }
            memset(buffer, 0, BUFFER_SIZE);
            valread = read(new_socket1, buffer, BUFFER_SIZE);
            qWrite2File(buffer, valread, 0);
            qlog_dbg("Received from PORT %d %d: %s\n", valread, g_qserver.dm_server_port, buffer);
            close(new_socket1);
        }

        if (FD_ISSET(server_fd2, &readfds))
        {
            if ((new_socket2 = accept(server_fd2, (struct sockaddr *)&address2, (socklen_t *)&addrlen)) < 0)
            {
                qlog_dbg("accept failed");
                exit(EXIT_FAILURE);
            }
            memset(buffer, 0, BUFFER_SIZE);
            valread = read(new_socket2, buffer, BUFFER_SIZE);
            qWrite2File(buffer, valread, 1);
            close(server_fd2);
            qlog_dbg("Received from PORT  %d %d: %s\n", valread, g_qserver.qdss_server_port, buffer);
            close(new_socket2);
        }

        if (FD_ISSET(server_fd3, &readfds))
        {
            if ((new_socket3 = accept(server_fd3, (struct sockaddr *)&address3, (socklen_t *)&addrlen)) < 0)
            {
                qlog_dbg("accept failed");
                exit(EXIT_FAILURE);
            }
            memset(buffer, 0, BUFFER_SIZE);
            valread = read(new_socket3, buffer, BUFFER_SIZE);
            qWrite2File(buffer, valread, 2);
            qlog_dbg("Received from PORT  %d %d: %s\n", valread, g_qserver.adpl_server_port, buffer);
            close(new_socket3);
        }
    }

    close(server_fd1);
    close(server_fd2);
    close(server_fd3);
    return;
}
*/
