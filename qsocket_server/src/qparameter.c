#include "common.h"
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <error.h>

unsigned qsocketCommonMsecs(void)
{
    static unsigned start = 0;
    unsigned now;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    now = (unsigned)ts.tv_sec * 1000 + (unsigned)(ts.tv_nsec / 1000000);
    if (start == 0) start = now;
    return now - start;
}

void print_usage()
{
    QLOGD("Usage: QSOCKET_SERVER [options]\n");
    QLOGD("Options:\n");
    QLOGD("  -h, --help            Show this help message\n");
    QLOGD("  -f, --string          Using special mask file path\n");
    QLOGD("  -t, --number          Socket client action time\n");
    QLOGD("  -m, --number          One log file max size (MB)\n");
    QLOGD("  -n, --number          Total number of log file saved\n");
    QLOGD("  -s, --string          -s 192.168.0.1-2500\n");
    QLOGD("  -T, --number          log type: qxdm:1 qxdm+qdss:2 qxdm+qdss+adpl:3\n");
    QLOGD("  -w, --string          work space: the path you will put the file\n");
}

void initParameter()
{
    memset(g_qparameter.cfg_file_path, '0', sizeof(g_qparameter.cfg_file_path));
    memset(g_qparameter.monitor_ip, '0', sizeof(g_qparameter.monitor_ip));
    memset(g_qparameter.workspace_path, '0', sizeof(g_qparameter.workspace_path));
    strcpy(g_qparameter.workspace_path, "./");
    g_qparameter.one_log_file_max_size = 512;
    g_qparameter.number_file = 512;
    g_qparameter.log_type = 1;
    g_qparameter.socket_clent_action_time = 0;
    g_qparameter.monitor_port = 0;
    return;
}
int parserArgv(int argc, char **argv)
{
    int opt;
    optind = 1;
    while (-1 != (opt = getopt(argc, argv, "hm:n:f:t:T:s:w:")))
    {
        if (opt == 'm' || opt == 'n' || opt == 'f' || opt == 't' || opt == 's' || opt == 'w')
        {
            if (optarg == NULL)
            {
                return -1;
            }
        }

        switch (opt)
        {
            case 'n':
                g_qparameter.number_file = atoi(optarg);
                if (g_qparameter.number_file < 0)
                    g_qparameter.number_file = 0;
                else if (g_qparameter.number_file > QSOCKET_LOGFILE_NUM)
                    g_qparameter.number_file = QSOCKET_LOGFILE_NUM;
                QLOGD("[n:]number file: %d\n", g_qparameter.number_file);
                break;

            case 'm':
                g_qparameter.one_log_file_max_size = atoi(optarg);
                if (g_qparameter.one_log_file_max_size < 1)
                    g_qparameter.one_log_file_max_size = 1;
                else if (g_qparameter.one_log_file_max_size > 1024)
                    g_qparameter.one_log_file_max_size = 1024;
                QLOGD("[m:]one log file max size: %d\n", g_qparameter.one_log_file_max_size);
                break;

            case 't':
                g_qparameter.socket_clent_action_time = atoi(optarg);
                QLOGD("[t:]socket clent action time: %d\n", g_qparameter.socket_clent_action_time);
                break;

            case 'f':
                strcpy(g_qparameter.cfg_file_path, optarg);
                QLOGD("[f:]cfg file path: %s\n", g_qparameter.cfg_file_path);
                break;

            case 'T':
                g_qparameter.log_type = atoi(optarg);
                QLOGD("[T:]log type: %d\n", g_qparameter.log_type);
                break;

            case 'w':
                strcpy(g_qparameter.workspace_path, optarg);
                QLOGD("[w:]log type: %s\n", g_qparameter.workspace_path);
                break;

            case 's': {
                char monitor_str[256];
                memset(monitor_str, '0', sizeof(monitor_str));
                strcpy(monitor_str, optarg);
                int res = parserMonitorMes(monitor_str);
                if (res < 0)
                {
                    QLOGD("error!");
                }
                QLOGD("[s:]log type: ip:[%s] port:[%d]\n", g_qparameter.monitor_ip, g_qparameter.monitor_port);
                break;
            }
            case 'h':
            default: print_usage(); return -1;
        }
    }
    return 0;
}