//
// Created by s on 19-5-14.
//

#include "session.h"
#include "ftpproto.h"
#include "privparent.h"
#include "privsock.h"
#include "sysutil.h"

void begin_session(session_t *sess)
{
    // 创建一对套接字，用于父子进程通信
    /*
    int sockfds[2];
    if( socketpair(AF_LOCAL,SOCK_STREAM,0,sockfds) < 0 )
    {
        ERR_EXIT("setsockpair");
    }
    */

    activate_oobinline(sess->ctrl_fd);

    priv_sock_init(sess);

    pid_t pid;
    // 创建服务进程，父进程为nobody进程
    pid = fork();

    switch (pid)
    {
        case -1: ERR_EXIT("fork service"); break;
        case 0:
            /*
            // service进程，使用sockfds[1]通信
            close(sockfds[0]);
            sess->child_fd = sockfds[1];
            */
            priv_sock_set_child_context(sess);
            handle_child(sess);
            break;
        default: {
            /*
            // nobody进程，使用sockfds[0]与子进程通信
            close(sockfds[1]);
            sess->parent_fd = sockfds[0];
            */
            priv_sock_set_parent_context(sess);
            handle_parent(sess);
            break;
        }
    }
}

void initFtpUserMessage(const char *ftp_user_message, session_t *sess)
{
    qserver_dbg("[%s][%d] entry [%s]\n", __func__, __LINE__, ftp_user_message);
    if (ftp_user_message == NULL) return;
    char *copy = strdup(ftp_user_message);
    if (copy == NULL)
    {
        free(copy);
        return;
    }

    char *tok = strtok(copy, "-");
    int num = 0;
    while (tok != NULL)
    {
        if (num == 0)
        {
            strcpy(sess->ftp_user_name, tok);
        }
        else
        {
            strcpy(sess->ftp_user_password, tok);
        }
        num += 1;
        tok = strtok(NULL, "-");
    }

    free(copy);
    qserver_dbg("leave user: [%s] pass: [%s]\n", sess->ftp_user_name, sess->ftp_user_password);
    return;
}