#include "qfile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "common.h"
#include <error.h>

static int cfg_file_fd;

void remove_oldest_file(const char *directory) { return; }

void *file_writer(void *arg) { return NULL; }

static int qxdm_fd = 0;
static int qdss_fd = 0;
static int adpl_fd = 0;

static size_t qxdm_size = 0;
static size_t qdss_size = 0;
static size_t adpl_size = 0;

int qreadFile(const char *filename)
{
    // cfg_file_fd
    cfg_file_fd = 0;
    cfg_file_fd = open(filename, O_RDONLY);
    if (cfg_file_fd < 0)
    {
        perror("File open failed");
        close(cfg_file_fd);
        return EXIT_FAILURE;
    }
    return cfg_file_fd;
}

void qcloseFile(int fd)
{
    if (fd < 0) return;
    close(fd);
    fd = -1;
    return;
}

unsigned qsocket_msecs(void)
{
    static unsigned start = 0;
    unsigned now;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    now = (unsigned)ts.tv_sec * 1000 + (unsigned)(ts.tv_nsec / 1000000);
    if (start == 0) start = now;
    return now - start;
}

static int write2QxdmFile(const char *data, int size);
static int write2QdssFile(const char *data, int size);
static int write2AdplFile(const char *data, int size);

static unsigned last_time = 0;
static unsigned now_time = 0;

static void initQfileTime()
{
    last_time = qsocket_msecs();
    now_time = last_time;
    return;
}

void initFile()
{
    initQfileTime();
    return;
}

int qwriteData2File(int type, const char *data, int size)
{
    int res = 0;
    switch (type)
    {
        case 1: res = write2QxdmFile(data, size); break;
        case 2: res = write2QdssFile(data, size); break;
        case 3: res = write2AdplFile(data, size); break;
        default: break;
    }
    now_time = qsocket_msecs();
    if (now_time >= (last_time + 5000))
    {
        QLOGD("time:[%d] recv type:[DIAG] data:[%zdM][%zdK][%zdB] \n", now_time, qxdm_size / (1 * 1024 * 1024), qxdm_size / 1024 % 1024, qxdm_size % 1024);
        if (type > 1)
        {
            QLOGD("type:[QDSS] data:[%zdM][%zdK][%zdB] \n", qdss_size / (1 * 1024 * 1024), qdss_size / 1024 % 1024, qdss_size % 1024);
        }
        if (type > 2)
        {
            QLOGD("type:[ADPL] data:[%zdM][%zdK][%zdB] \n", adpl_size / (1 * 1024 * 1024), adpl_size / 1024 % 1024, adpl_size % 1024);
        }
        last_time = now_time;
    }
    return res;
}

static int checkFileSize(int type);
static int createLogFile(int type);
static void closeFile(int type);

static int write2QxdmFile(const char *data, int size)
{
    // QLOGD("[%s][%d] entry! fd:[%d] all_size:[%ld]\n", __func__, __LINE__, qxdm_fd, qxdm_size);
    if (qxdm_fd == 0)
    {
        qxdm_fd = createLogFile(1);
    }
    if (qxdm_fd < 0)
    {
        return -1;
    }

    int res = write(qxdm_fd, data, size);
    if (res != size)
    {
        return -1;
    }

    qxdm_size += res;
    res = checkFileSize(1);
    if (res > 0)
    {
        closeFile(1);
    }
    return 0;
}

static int write2QdssFile(const char *data, int size)
{
    if (qdss_fd == 0)
    {
        qdss_fd = createLogFile(2);
    }
    if (qdss_fd < 0)
    {
        return -1;
    }

    int res = write(qdss_fd, data, size);
    if (res != size)
    {
        return -1;
    }

    qdss_size += res;
    res = checkFileSize(2);
    if (res > 0)
    {
        closeFile(2);
    }
    return 0;
}

static int write2AdplFile(const char *data, int size)
{
    if (adpl_fd == 0)
    {
        adpl_fd = createLogFile(3);
    }
    if (adpl_fd < 0)
    {
        return -1;
    }

    int res = write(adpl_fd, data, size);
    if (res != size)
    {
        return -1;
    }

    adpl_size += res;
    res = checkFileSize(3);
    if (res > 0)
    {
        closeFile(3);
    }
    return 0;
}

static char *getNewTime()
{
    static char time_name[100];
    time_t ltime;
    struct tm *currtime;

    time(&ltime);
    currtime = localtime(&ltime);

    snprintf(time_name, sizeof(time_name), "%04d%02d%02d_%02d%02d%02d", (currtime->tm_year + 1900), (currtime->tm_mon + 1), currtime->tm_mday, currtime->tm_hour, currtime->tm_min,
             currtime->tm_sec);
    return time_name;
}

static int createLogFile(int type)
{
    //
    int file_fd = 0;
    char path[1024];
    memset(path, '0', sizeof(path));
    strcpy(path, g_qparameter.workspace_path);
    strcat(path, getNewTime());
    switch (type)
    {
        case 1: strcat(path, ".qmdl2"); break;
        case 2: strcat(path, "_qdss.bin"); break;
        case 3: strcat(path, ".adplv4"); break;

        default: break;
    }

    file_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (file_fd < 0)
    {
        QLOGD("file open error \n");
        return -1;
    }

    return file_fd;
}

static int checkFileSize(int type)
{
    unsigned int num = 0;
    switch (type)
    {
        case 1:
            if (qxdm_size < (1024 * 1024))
                num = 0;
            else
                num = qxdm_size / (1024 * 1024);
            break;
        case 2:
            if (qxdm_size < (1024 * 1024))
                num = 0;
            else
                num = qdss_size / (1024 * 1024);
            break;
        case 3:
            if (qxdm_size < (1024 * 1024))
                num = 0;
            else
                num = adpl_size / (1024 * 1024);
            break;
        default: break;
    }

    // QLOGD("[%s][%d] entry! num:[%d] set max size:[%d]\n", __func__, __LINE__, num, g_qparameter.one_log_file_max_size);
    return num > g_qparameter.one_log_file_max_size ? 1 : -1;
}

static void closeFile(int type)
{
    switch (type)
    {
        case 1:
            close(qxdm_fd);
            qxdm_fd = 0;
            break;
        case 2:
            close(qdss_fd);
            qdss_fd = 0;
            break;
        case 3:
            close(adpl_fd);
            adpl_fd = 0;
            break;

        default: break;
    }
    return;
}