/*
    Copyright 2023 Quectel Wireless Solutions Co.,Ltd

    Quectel hereby grants customers of Quectel a license to use, modify,
    distribute and publish the Software in binary form provided that
    customers shall have no right to reverse engineer, reverse assemble,
    decompile or reduce to source code form any portion of the Software.
    Under no circumstances may customers modify, demonstrate, use, deliver
    or disclose any portion of the Software in source code form.
*/

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <termios.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include <malloc.h>
#include <stdbool.h>
#include <pthread.h>
#include <inttypes.h>
#include <sys/socket.h>

/* modify macro MIN
 * usually we difine it as: (a) < (b) ? (a) : (b)
 * but it will cause some problems, here is a case:
 * MIN(i++, j++), when calling the macro above, i++ will be run two times, which is wrong.
 * so we can modify it as following.
 *  (void)(&_a == &_b); is use to check wether the type of 'a' and 'b' is same or not.
 * (void) is used to eliminated warnning.
 */
#define MIN(a, b)           \
    ({                      \
        typeof(a) _a = a;   \
        typeof(b) _b = b;   \
        (void)(&_a == &_b); \
        _a < _b ? _a : _b;  \
    })

#include "sahara.h"
#include "qlog.h"

q_sahara_data_t sahara_data = {
    NULL,                // q_rx_buf
    NULL,                // q_tx_buf
    NULL,                // q_misc_buf
    Q_SAHARA_WAIT_ONE,   // q_state
    0,                   // timed_data_size
    -1,                  // fd
    -1,                  // ram_dump_image
    5,                   // max_ram_dump_retries
    Q_SAHARA_RAW_BUF_SZ, // q_max_ram_dump_read
    Q_SAHARA_MODE_FOUR,  // q_mode
    Q_SAHARA_MODE_FOUR,  // prev_mode
    0,                   // q_cmd
    false                // q_ram_dump_64bit
};

typedef struct
{
    const char *port_name;
    int port_fd;
    int rx_timeout;
    size_t MAX_TO_READ;
    size_t MAX_TO_WRITE;
} com_port_t;

static com_port_t com_port = {
    "/dev/ttyUSB0", // port_name
    -1,             // port_fd
    5,              // rx_timeout
    1024 * 64,
    1024 * 64,
};

typedef struct
{
    const char *path_to_save_files;
    int verbose;
    int do_reset;
} kickstart_options_t;

static kickstart_options_t kickstart_options = {
    NULL, // path_to_save_files
    1,    // verbose
    1,
};

enum LOG_LEVEL
{
    LOG_DEBUG = 1,
    LOG_EVENT,
    LOG_INFO,
    LOG_STATUS,
    LOG_WARN,
    LOG_ERROR
};

extern unsigned qlog_msecs(void);
#define dbg(log_level, fmt, arg...)                                           \
    do                                                                        \
    {                                                                         \
        if (kickstart_options.verbose || LOG_ERROR == log_level)              \
        {                                                                     \
            unsigned msec = qlog_msecs();                                     \
            printf("[%03d.%03d] " fmt "\n", msec / 1000, msec % 1000, ##arg); \
        }                                                                     \
    } while (0)

static int wakeUpDevice()
{
    const unsigned char wake_up_buff[] = {0x0c, 0x14, 0x3a, 0x7e};
    size_t bytes_sent = qlog_poll_write(com_port.port_fd, wake_up_buff, sizeof(wake_up_buff), 1000);
    return (bytes_sent == sizeof(wake_up_buff));
}

static bool port_tx_data(void *buffer, size_t bytes_to_send)
{
    size_t bytes_sent = qlog_poll_write(com_port.port_fd, buffer, bytes_to_send, 1000);
    return (bytes_sent == bytes_to_send);
}

static bool port_rx_data(void *buffer, size_t bytes_to_read, size_t *bytes_read)
{
    fd_set rfds;
    struct timeval tv;
    int retval;

    if (bytes_to_read == 0)
    {
        *bytes_read = 0;
        return true;
    }

    // Init read file descriptor
    FD_ZERO(&rfds);
    FD_SET(com_port.port_fd, &rfds);

    // time out initializtion.
    tv.tv_sec = com_port.rx_timeout >= 0 ? com_port.rx_timeout : 0;
    tv.tv_usec = 0;

    retval = select(com_port.port_fd + 1, &rfds, NULL, NULL, ((com_port.rx_timeout >= 0) ? (&tv) : (NULL)));
    if (retval <= 0)
    {
        dbg(LOG_ERROR, "select returned error: %s", strerror(errno));
        return false;
    }

    retval = read(com_port.port_fd, buffer, MIN(bytes_to_read, com_port.MAX_TO_READ));
    if (retval <= 0)
    {
        dbg(LOG_ERROR, "Read/Write File descriptor returned error: %s, error code %d", strerror(errno), retval);
        return false;
    }

    if (NULL != bytes_read) *bytes_read = retval;

    return true;
}

static bool sahara_tx_data(size_t bytes_to_send) { return port_tx_data(sahara_data.q_tx_buf, bytes_to_send); }

static bool sahara_rx_data(size_t bytes_to_read)
{
    q_sahara_pkt_h *command_packet_header = NULL;
    size_t temp_bytes_read = 0, bytes_read = 0;

    const char *boot_sahara_cmd_id_str[Q_SAHARA_NINETEEN] = {
        "Q_SAHARA_ZERO",     "Q_SAHARA_ONE",     "Q_SAHARA_TWO",     "Q_SAHARA_THREE",     "Q_SAHARA_FOUR",     "Q_SAHARA_FIVE",    "Q_SAHARA_SIX",
        "Q_SAHARA_SEVEN",    "Q_SAHARA_EIGTH",   "Q_SAHARA_NINE",    "Q_SAHARA_TEN",       "Q_SAHARA_ELEVEN",   "Q_SAHARA_TWELEVE", "Q_SAHARA_THIRTEEN",
        "Q_SAHARA_FOURTEEN", "Q_SAHARA_FIFTEEN", "Q_SAHARA_SIXTEEN", "Q_SAHARA_SEVENTEEN", "Q_SAHARA_EIGHTEEN",
    };

    if (0 == bytes_to_read)
    {
        command_packet_header = (q_sahara_pkt_h *)sahara_data.q_rx_buf;
        memset(command_packet_header, 0x00, sizeof(q_sahara_pkt_h));

        if (false == port_rx_data(sahara_data.q_rx_buf, sizeof(q_sahara_pkt_h), &temp_bytes_read)) return false;

        dbg(LOG_INFO, "Read %zd bytes, q_cmd %d and packet q_len %d bytes", temp_bytes_read, qlog_le32(command_packet_header->q_cmd), qlog_le32(command_packet_header->q_len));
        if (temp_bytes_read != sizeof(q_sahara_pkt_h)) return false;

        if (qlog_le32(command_packet_header->q_cmd) < Q_SAHARA_NINETEEN)
        {
            dbg(LOG_EVENT, "RECEIVED <-- %s", boot_sahara_cmd_id_str[qlog_le32(command_packet_header->q_cmd)]);
            if (false == port_rx_data(sahara_data.q_rx_buf + sizeof(q_sahara_pkt_h), qlog_le32(command_packet_header->q_len) - sizeof(q_sahara_pkt_h), &temp_bytes_read))
                return false;
            if (temp_bytes_read != (qlog_le32(command_packet_header->q_len) - sizeof(q_sahara_pkt_h)))
            {
                dbg(LOG_INFO, "Read %zd bytes", temp_bytes_read + sizeof(q_sahara_pkt_h));
                return false;
            }
        }
        else
        {
            dbg(LOG_EVENT, "RECEIVED <-- SAHARA_CMD_UNKONOW_%d", qlog_le32(command_packet_header->q_cmd));
            return false;
        }
    }
    else
    {
        while (bytes_read < bytes_to_read)
        {
            if (false == port_rx_data(sahara_data.q_rx_buf + bytes_read, bytes_to_read - bytes_read, &temp_bytes_read))
            {
                dbg(LOG_ERROR, "bytes_read = %zd, bytes_to_read = %zd", bytes_read, bytes_to_read);
                return false;
            }
            else
                bytes_read += temp_bytes_read;
        }
    }

    return true;
}

static int timeval_subtract(struct timeval *result, struct timeval *x, struct timeval *y)
{
    // Perform the carry for the later subtraction by updating y.
    if (x->tv_usec < y->tv_usec)
    {
        int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
        y->tv_usec -= 1000000 * nsec;
        y->tv_sec += nsec;
    }
    if (x->tv_usec - y->tv_usec > 1000000)
    {
        int nsec = (x->tv_usec - y->tv_usec) / 1000000;
        y->tv_usec += 1000000 * nsec;
        y->tv_sec -= nsec;
    }

    // Compute the time remaining to wait. tv_usec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;

    // Return 1 if result is negative.
    return x->tv_sec < y->tv_sec;
}

static void time_throughput_calculate(struct timeval *start_time, struct timeval *end_time, size_t size_bytes)
{
    struct timeval result;
    double TP = 0.0;

    if (size_bytes == 0)
    {
        dbg(LOG_INFO, "Cannot calculate throughput, size is 0");
        return;
    }
    timeval_subtract(&result, end_time, start_time);

    TP = (double)result.tv_usec / 1000000.0;
    TP += (double)result.tv_sec;

    if (TP > 0.0)
    {
        TP = (double)((double)size_bytes / TP) / (1024.0 * 1024.0);
        dbg(LOG_STATUS, "%zd bytes transferred in %ld.%06ld seconds (%.4fMBps)", size_bytes, result.tv_sec, result.tv_usec, TP);
    }
    else
        dbg(LOG_STATUS, "%zd bytes transferred in %ld.%06ld seconds", size_bytes, result.tv_sec, result.tv_usec);
}

static bool send_reset_command()
{
    struct sahara_pkt *sahara_reset;
    sahara_reset = (struct sahara_pkt *)sahara_data.q_tx_buf;
    sahara_reset->q_header.q_cmd = qlog_le32(Q_SAHARA_SEVEN);
    sahara_reset->q_header.q_len = qlog_le32(sizeof(sahara_reset->q_sahara_reset_pkt) + sizeof(q_sahara_pkt_h));

    /* Send the Reset Request */
    dbg(LOG_EVENT, "===> SAHARA_RESET");
    if (false == sahara_tx_data(sizeof(sahara_reset->q_sahara_reset_pkt) + sizeof(q_sahara_pkt_h)))
    {
        dbg(LOG_ERROR, "Sending RESET packet failed");
        return false;
    }

    return true;
}

static bool send_hello_response(void *q_tx_buf, const struct sahara_pkt *sahara_hello)
{
    struct sahara_pkt *sahara_hello_resp;
    sahara_hello_resp = (struct sahara_pkt *)q_tx_buf;

    sahara_hello_resp->q_header.q_cmd = qlog_le32(Q_SAHARA_TWO);
    sahara_hello_resp->q_header.q_len = qlog_le32(sizeof(sahara_hello_resp->q_sahara_hello_pkt_response) + sizeof(q_sahara_pkt_h));
    sahara_hello_resp->q_sahara_hello_pkt_response.q_ver = sahara_hello->q_sahara_hello_pkt.q_ver;
    sahara_hello_resp->q_sahara_hello_pkt_response.q_ver_sup = sahara_hello->q_sahara_hello_pkt.q_ver_sup;
    sahara_hello_resp->q_sahara_hello_pkt_response.q_status = qlog_le32(Q_SAHARA_STATUS_ZERO);
    sahara_hello_resp->q_sahara_hello_pkt_response.q_mode = sahara_hello->q_sahara_hello_pkt.q_mode;
    sahara_hello_resp->q_sahara_hello_pkt_response.q_reserve1 = qlog_le32(1);
    sahara_hello_resp->q_sahara_hello_pkt_response.q_reserve2 = qlog_le32(2);
    sahara_hello_resp->q_sahara_hello_pkt_response.q_reserve3 = qlog_le32(3);
    sahara_hello_resp->q_sahara_hello_pkt_response.q_reserve4 = qlog_le32(4);
    sahara_hello_resp->q_sahara_hello_pkt_response.q_reserve5 = qlog_le32(5);
    sahara_hello_resp->q_sahara_hello_pkt_response.q_reserve6 = qlog_le32(6);

    if (qlog_le32(sahara_hello->q_sahara_hello_pkt.q_mode) != sahara_data.q_mode)
    {
        dbg(LOG_ERROR, "Not expect module q_state ");
        return false;
    }

    /*Send the Hello  Resonse Request*/
    dbg(LOG_EVENT, "===> Q_SAHARA_TWO");
    if (false == sahara_tx_data(sizeof(sahara_hello_resp->q_sahara_hello_pkt_response) + sizeof(q_sahara_pkt_h)))
    {
        dbg(LOG_ERROR, "Tx Sahara Data Failed ");
        return false;
    }
    sahara_data.q_state = Q_SAHARA_WAIT_TWO;

    return true;
}

static bool send_memory_read_packet(uint64_t memory_table_address, uint64_t q_memory_table_length)
{
    struct sahara_pkt *sahara_memory_read;
    struct sahara_pkt *sahara_memory_read_64bit;
    sahara_memory_read = (struct sahara_pkt *)sahara_data.q_tx_buf;
    sahara_memory_read_64bit = (struct sahara_pkt *)sahara_data.q_tx_buf;

    if (true == sahara_data.q_ram_dump_64bit)
    {
        dbg(LOG_EVENT, "===>  Q_SAHARA_SEVENTEEN, address 0x%08" PRIX64 ", q_len 0x%08" PRIX64, memory_table_address, q_memory_table_length);
        sahara_memory_read_64bit->q_header.q_cmd = qlog_le32(Q_SAHARA_SEVENTEEN);
        sahara_memory_read_64bit->q_header.q_len = qlog_le32(sizeof(sahara_memory_read_64bit->q_sahara_pkt_memory_read_64bit) + sizeof(q_sahara_pkt_h));
        sahara_memory_read_64bit->q_sahara_pkt_memory_read_64bit.q_memory_addr = qlog_le64(memory_table_address);
        sahara_memory_read_64bit->q_sahara_pkt_memory_read_64bit.q_memory_length = qlog_le64(q_memory_table_length);

        /* Send the Memory Read packet */
        if (false == sahara_tx_data(sizeof(sahara_memory_read_64bit->q_sahara_pkt_memory_read_64bit) + sizeof(q_sahara_pkt_h)))
        {
            dbg(LOG_ERROR, "Sending MEMORY_READ packet failed");
            return false;
        }
    }
    else
    {
        dbg(LOG_EVENT, "===>  Q_SAHARA_TEN, address 0x%08" PRIX64 ", q_len 0x%08" PRIX64, memory_table_address, q_memory_table_length);
        sahara_memory_read->q_header.q_cmd = qlog_le32(Q_SAHARA_TEN);
        sahara_memory_read->q_header.q_len = qlog_le32(sizeof(sahara_memory_read->q_sahara_pkt_memory_read) + sizeof(q_sahara_pkt_h));
        sahara_memory_read->q_sahara_pkt_memory_read.q_memory_addr = qlog_le32((uint32_t)memory_table_address);
        sahara_memory_read->q_sahara_pkt_memory_read.q_memory_length = qlog_le32((uint32_t)q_memory_table_length);

        /* Send the Memory Read packet */
        if (false == sahara_tx_data(sizeof(sahara_memory_read->q_sahara_pkt_memory_read) + sizeof(q_sahara_pkt_h)))
        {
            dbg(LOG_ERROR, "Sending MEMORY_READ packet failed");
            return false;
        }
    }

    return true;
}

static bool is_valid_memory_table(uint64_t memory_table_size)
{
    if (true == sahara_data.q_ram_dump_64bit && memory_table_size % sizeof(q_debug_type_64bit) == 0)
    {
        return true;
    }
    else if (false == sahara_data.q_ram_dump_64bit && memory_table_size % sizeof(q_debug_type) == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

static bool sahara_read_dump(void)
{
    int retval = 0;
    int num_debug_entries = -1;
    int i = 0;
    uint64_t q_memory_table_addr = 0;
    uint64_t q_memory_table_length = 0;

    struct timeval time_start, time_end;

    struct sahara_pkt *pr_sahara_pkt;

    pr_sahara_pkt = (struct sahara_pkt *)sahara_data.q_rx_buf;

    q_debug_type *sahara_memory_table_rx = (q_debug_type *)sahara_data.q_rx_buf;
    q_debug_type_64bit *sahara_memory_table = (q_debug_type_64bit *)sahara_data.q_misc_buf;

    sahara_data.q_state = Q_SAHARA_WAIT_ONE;
    kickstart_options.verbose = 1;

    while (1)
    {
        switch (sahara_data.q_state)
        {
            case Q_SAHARA_WAIT_ONE:
                dbg(LOG_EVENT, "<=== Q_SAHARA_WAIT_ONE");
                sahara_tx_data(1);
                if (false == sahara_rx_data(0)) // size 0 means we don't know what to expect. So we'll just try to read the 8 byte q_header
                {
                    if (false == wakeUpDevice())
                    {
                        dbg(LOG_ERROR, "send wake up error!");
                        return false;
                    }
                    if (false == sahara_rx_data(0)) return false;
                }

                // Check if the received q_cmd is a hello q_cmd
                if (Q_SAHARA_ONE != qlog_le32(pr_sahara_pkt->q_header.q_cmd))
                {
                    dbg(LOG_ERROR, "Received a different q_cmd: %x while waiting for hello packet", qlog_le32(pr_sahara_pkt->q_header.q_cmd));
                    if (false == send_reset_command())
                    {
                        return false;
                    }
                    // set the q_state to Q_SAHARA_WAIT_THREE
                    dbg(LOG_EVENT, "<=== Q_SAHARA_WAIT_THREE\n");
                    sahara_data.q_state = Q_SAHARA_WAIT_THREE;
                }
                else
                {
                    if (false == send_hello_response(sahara_data.q_tx_buf, pr_sahara_pkt))
                    {
                        dbg(LOG_ERROR, "send_hello_response failed\n");
                        return false;
                    }
                }
                break;

            case Q_SAHARA_WAIT_TWO:
                dbg(LOG_INFO, "<=== Q_SAHARA_WAIT_TWO");
                if (false == sahara_rx_data(0)) return false;

                // Check if it is  an end of image Tx
                if (Q_SAHARA_NINE == qlog_le32(((q_sahara_pkt_h *)sahara_data.q_rx_buf)->q_cmd) || Q_SAHARA_SIXTEEN == qlog_le32(((q_sahara_pkt_h *)sahara_data.q_rx_buf)->q_cmd))
                {
                    dbg(LOG_EVENT, "<=== Q_SAHARA_NINE");

                    if (Q_SAHARA_SIXTEEN == qlog_le32(((q_sahara_pkt_h *)sahara_data.q_rx_buf)->q_cmd))
                    {
                        sahara_data.q_ram_dump_64bit = true;
                        dbg(LOG_EVENT, "Using 64 bit RAM dump q_mode");
                        q_memory_table_addr = qlog_le64(pr_sahara_pkt->q_sahara_memory_pkt_debug_64bit.q_memory_table_addr);
                        q_memory_table_length = qlog_le64(pr_sahara_pkt->q_sahara_memory_pkt_debug_64bit.q_memory_table_length);
                    }
                    else
                    {
                        sahara_data.q_ram_dump_64bit = false;
                        q_memory_table_addr = qlog_le32(pr_sahara_pkt->q_sahara_memory_pkt_debug.q_memory_table_addr);
                        q_memory_table_length = qlog_le32(pr_sahara_pkt->q_sahara_memory_pkt_debug.q_memory_table_length);
                    }

                    dbg(LOG_INFO, "Memory Table Address: 0x%08" PRIX64 ", Memory Table q_len: 0x%08" PRIX64, q_memory_table_addr, q_memory_table_length);

                    if (false == is_valid_memory_table(q_memory_table_length))
                    {
                        dbg(LOG_ERROR, "Invalid memory table received");
                        if (false == send_reset_command())
                        {
                            return false;
                        }
                        sahara_data.q_state = Q_SAHARA_WAIT_THREE;
                        break;
                    }

                    if (q_memory_table_length > 0)
                    {
                        if (false == send_memory_read_packet(q_memory_table_addr, q_memory_table_length))
                        {
                            return false;
                        }

                        if (q_memory_table_length > Q_SAHARA_RAW_BUF_SZ)
                        {
                            dbg(LOG_ERROR, "Memory table q_len is greater than size of intermediate buffer");
                            return false;
                        }
                    }
                    dbg(LOG_EVENT, "<=== Q_SAHARA_WAIT_SEVEN");
                    sahara_data.q_state = Q_SAHARA_WAIT_SEVEN;
                }
                else
                {
                    dbg(LOG_ERROR, "Received an unknown q_cmd: %d ", qlog_le32(((q_sahara_pkt_h *)sahara_data.q_rx_buf)->q_cmd));
                    if (Q_SAHARA_ONE == qlog_le32(((q_sahara_pkt_h *)sahara_data.q_rx_buf)->q_cmd)) continue;
                    if (false == send_reset_command())
                    {
                        return false;
                    }
                    // set the q_state to Q_SAHARA_WAIT_THREE
                    dbg(LOG_EVENT, "<=== Q_SAHARA_WAIT_THREE");
                    sahara_data.q_state = Q_SAHARA_WAIT_THREE;
                }
                break;

            case Q_SAHARA_WAIT_SEVEN:
                dbg(LOG_INFO, "<=== Q_SAHARA_WAIT_SEVEN");
                num_debug_entries = 0;
                if (q_memory_table_length > 0)
                {
                    if (false == sahara_rx_data((size_t)q_memory_table_length))
                    {
                        return false;
                    }
                    dbg(LOG_INFO, "Memory Debug table received");

                    if (true == sahara_data.q_ram_dump_64bit)
                    {
                        memcpy(sahara_data.q_misc_buf, sahara_data.q_rx_buf, (size_t)q_memory_table_length);
                        num_debug_entries = (int)(q_memory_table_length / sizeof(q_debug_type_64bit));
                    }
                    else
                    {
                        num_debug_entries = (int)(q_memory_table_length / sizeof(q_debug_type));
                        if (num_debug_entries * sizeof(q_debug_type_64bit) > Q_SAHARA_RAW_BUF_SZ)
                        {
                            dbg(LOG_ERROR, "q_len of memory table converted to 64-bit entries is greater than size of intermediate buffer");
                            return false;
                        }

                        for (i = 0; i < num_debug_entries; ++i)
                        {
                            sahara_memory_table[i].q_save_pref = (uint64_t)qlog_le32(sahara_memory_table_rx[i].q_save_pref);
                            sahara_memory_table[i].q_mem_base = (uint64_t)qlog_le32(sahara_memory_table_rx[i].q_mem_base);
                            sahara_memory_table[i].q_len = (uint64_t)qlog_le32(sahara_memory_table_rx[i].q_len);
                            strncpy(sahara_memory_table[i].q_filename, sahara_memory_table_rx[i].q_filename, DLOAD_DEBUG_STRLEN_BYTES);
                            strncpy(sahara_memory_table[i].q_desc, sahara_memory_table_rx[i].q_desc, DLOAD_DEBUG_STRLEN_BYTES);
                        } // end for (i = 0; i < num_debug_entries; ++i)
                    }
                }

                for (i = 0; i < num_debug_entries; i++)
                {
                    dbg(LOG_EVENT, "Base 0x%08" PRIX64 " Len 0x%08" PRIX64 ", '%s', '%s'", sahara_memory_table[i].q_mem_base, sahara_memory_table[i].q_len,
                        sahara_memory_table[i].q_filename, sahara_memory_table[i].q_desc);
                }
                sahara_data.q_state = Q_SAHARA_WAIT_EIGHT;
                break;

            case Q_SAHARA_WAIT_EIGHT:
                dbg(LOG_INFO, "<=== Q_SAHARA_WAIT_EIGHT");
                for (i = 0; i < num_debug_entries; i++)
                {
                    uint64_t cur = 0;
                    int fd = -1;
                    char full_filename[255] = {0};
                    if (kickstart_options.path_to_save_files)
                    {
                        strncpy(full_filename, kickstart_options.path_to_save_files, 250);
                        strcat(full_filename, "/");
                    }
                    strcat(full_filename, sahara_memory_table[i].q_filename);

                    fd = qlog_logfile_create_fullname(0, full_filename, sahara_memory_table[i].q_len, 1);
                    if (fd == -1)
                    {
                        dbg(LOG_ERROR, "ERROR: Your file '%s' does not exist or cannot be created\n\n", sahara_memory_table[num_debug_entries].q_filename);
                        exit(0);
                    }
                    gettimeofday(&time_start, NULL);

                    while (cur < sahara_memory_table[i].q_len)
                    {
                        uint64_t len = MIN((uint32_t)(sahara_memory_table[i].q_len - cur), sahara_data.q_max_ram_dump_read);

                        if (len < sahara_data.q_max_ram_dump_read || cur == 0 || (cur % (16 * 1024 * 1024)) == 0)
                            kickstart_options.verbose = 1;
                        else
                            kickstart_options.verbose = 0;

                        retval = send_memory_read_packet(sahara_memory_table[i].q_mem_base + cur, len);
                        if (false == retval)
                        {
                            dbg(LOG_ERROR, "send_memory_read_packet failed: %s", strerror(errno));
                            qlog_logfile_close(fd);
                            return false;
                        }

                        retval = sahara_rx_data((size_t)len);
                        if (false == retval)
                        {
                            if (sahara_data.q_max_ram_dump_read > (16 * 1024))
                            {
                                sahara_data.q_max_ram_dump_read = sahara_data.q_max_ram_dump_read / 2;
                                continue;
                            }
                            dbg(LOG_ERROR, "sahara_rx_data failed: %s", strerror(errno));
                            qlog_logfile_close(fd);
                            return false;
                        }

                        cur += len;
                        retval = qlog_logfile_save(fd, sahara_data.q_rx_buf, len);

                        if (retval < 0)
                        {
                            dbg(LOG_ERROR, "file write failed: %s", strerror(errno));
                            qlog_logfile_close(fd);
                            return false;
                        }
                        if ((uint32_t)retval != len)
                        {
                            dbg(LOG_WARN, "Wrote only %d of 0x%08" PRIX64 " bytes", retval, q_memory_table_length);
                        }
                    }

                    kickstart_options.verbose = 1;
                    dbg(LOG_STATUS, "Received file '%s'", sahara_memory_table[i].q_filename);
                    qlog_logfile_close(fd);
                    gettimeofday(&time_end, NULL);
                    time_throughput_calculate(&time_start, &time_end, sahara_memory_table[i].q_len);
                }

                if (kickstart_options.do_reset)
                {
                    if (false == send_reset_command())
                    {
                        return false;
                    }
                    sahara_data.q_state = Q_SAHARA_WAIT_THREE;
                }
                else
                {
                    return true;
                }
                break;

            case Q_SAHARA_WAIT_FOUR:
                dbg(LOG_EVENT, "<=== Q_SAHARA_WAIT_FOUR");
                return false;
                break;

            case Q_SAHARA_WAIT_THREE:
                dbg(LOG_EVENT, "<=== Q_SAHARA_WAIT_THREE");
                if (true == sahara_rx_data(0))
                {
                    if (Q_SAHARA_EIGTH != qlog_le32(pr_sahara_pkt->q_header.q_cmd))
                    {
                        dbg(LOG_INFO, "Waiting for reset response code %i, received %i instead.", Q_SAHARA_EIGTH, qlog_le32(pr_sahara_pkt->q_header.q_cmd));
                        continue;
                    }
                }
                else
                {
                    if (Q_SAHARA_EIGTH == qlog_le32(pr_sahara_pkt->q_header.q_cmd))
                    {
                        dbg(LOG_INFO, "Get reset response code %i", pr_sahara_pkt->q_header.q_cmd);
                        return true;
                    }
                    else
                    {
                        dbg(LOG_ERROR, "read failed: Linux system error: %s", strerror(errno));
                        return false;
                    }
                }

                return true;
                break;

            default: dbg(LOG_ERROR, "Unrecognized q_state %d", sahara_data.q_state); return false;
        } /* end switch */
    }     /* end while (1) */
}

int sahara_catch_dump(int port_fd, const char *path_to_save_files, int do_reset)
{
    int retval;

    sahara_data.q_mode = Q_SAHARA_MODE_TWO;
    com_port.port_fd = port_fd;
    kickstart_options.path_to_save_files = path_to_save_files;
    kickstart_options.do_reset = do_reset;

    sahara_data.q_rx_buf = malloc(Q_SAHARA_RAW_BUF_SZ);
    sahara_data.q_tx_buf = malloc(2048);
    sahara_data.q_misc_buf = malloc(Q_SAHARA_RAW_BUF_SZ); // 2048 is not enough for SDX62 Memory Table q_len: 0x000007EC

    if (NULL == sahara_data.q_rx_buf || NULL == sahara_data.q_tx_buf || NULL == sahara_data.q_misc_buf)
    {
        dbg(LOG_ERROR, "Failed to allocate sahara buffers");
        return false;
    }

    retval = sahara_read_dump();
    if (false == retval)
    {
        dbg(LOG_ERROR, "Sahara protocol error");
    }
    else
    {
        dbg(LOG_ERROR, "Sahara protocol completed");
    }

    free(sahara_data.q_rx_buf);
    free(sahara_data.q_tx_buf);
    free(sahara_data.q_misc_buf);

    sahara_data.q_rx_buf = sahara_data.q_tx_buf = sahara_data.q_misc_buf = NULL;

    if (retval == false)
        dbg(LOG_INFO, "Catch DUMP using Sahara protocol failed\n\n");
    else
        dbg(LOG_INFO, "Catch DUMP using Sahara protocol successful\n\n");

    return retval;
}
