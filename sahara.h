/*
    Copyright 2023 Quectel Wireless Solutions Co.,Ltd

    Quectel hereby grants customers of Quectel a license to use, modify,
    distribute and publish the Software in binary form provided that
    customers shall have no right to reverse engineer, reverse assemble,
    decompile or reduce to source code form any portion of the Software.
    Under no circumstances may customers modify, demonstrate, use, deliver
    or disclose any portion of the Software in source code form.
*/

#ifndef SAHARA_H
#define SAHARA_H

// #define Q_SAHARA_RAW_BUF_SZ (32*1024)
#define Q_SAHARA_RAW_BUF_SZ (128 * 1024)

#define Q_SAHARA_ONE 0x01
#define Q_SAHARA_TWO 0x02
#define Q_SAHARA_SEVEN 0x07
#define Q_SAHARA_EIGTH 0x08
#define Q_SAHARA_NINE 0x09
#define Q_SAHARA_TEN 0x0A
#define Q_SAHARA_SIXTEEN 0x10
#define Q_SAHARA_SEVENTEEN 0x11
#define Q_SAHARA_NINETEEN 0x13

#define Q_SAHARA_STATUS_ZERO 0x00

typedef enum
{
    Q_SAHARA_MODE_ZERO = 0x0,
    Q_SAHARA_MODE_ONE = 0x1,
    Q_SAHARA_MODE_TWO = 0x2,
    Q_SAHARA_MODE_THREE = 0x3,
    Q_SAHARA_MODE_FOUR = 0x4,
} q_sahara_mode;

typedef enum
{
    Q_SAHARA_WAIT_ONE,
    Q_SAHARA_WAIT_TWO,
    Q_SAHARA_WAIT_THREE,
    Q_SAHARA_WAIT_FOUR,
    Q_SAHARA_WAIT_FIVE,
    Q_SAHARA_WAIT_SIX,
    Q_SAHARA_WAIT_SEVEN,
    Q_SAHARA_WAIT_EIGHT
} q_sahara_state;

typedef struct
{
    uint32_t q_cmd;
    uint32_t q_len;
} q_sahara_pkt_h;

struct sahara_pkt
{
    q_sahara_pkt_h q_header;

    union
    {
        struct
        {
            uint32_t q_ver;
            uint32_t q_ver_sup;
            uint32_t q_cmd_pkt_len;
            uint32_t q_mode;
        } q_sahara_hello_pkt;
        struct
        {
            uint32_t q_ver;
            uint32_t q_ver_sup;
            uint32_t q_status;
            uint32_t q_mode;
            uint32_t q_reserve1;
            uint32_t q_reserve2;
            uint32_t q_reserve3;
            uint32_t q_reserve4;
            uint32_t q_reserve5;
            uint32_t q_reserve6;
        } q_sahara_hello_pkt_response;
        struct
        {
            uint32_t q_memory_table_addr;
            uint32_t q_memory_table_length;
        } q_sahara_memory_pkt_debug;
        struct
        {
            uint64_t q_memory_table_addr;
            uint64_t q_memory_table_length;
        } q_sahara_memory_pkt_debug_64bit;
        struct
        {
            uint32_t q_memory_addr;
            uint32_t q_memory_length;
        } q_sahara_pkt_memory_read;
        struct
        {
            uint64_t q_memory_addr;
            uint64_t q_memory_length;
        } q_sahara_pkt_memory_read_64bit;
        struct
        {
        } q_sahara_reset_pkt;
        struct
        {
        } q_sahara_reset_pkt_response;
    };
};

#define DLOAD_DEBUG_STRLEN_BYTES 20
typedef struct
{
    uint32_t q_save_pref;
    uint32_t q_mem_base;
    uint32_t q_len;
    char q_desc[DLOAD_DEBUG_STRLEN_BYTES];
    char q_filename[DLOAD_DEBUG_STRLEN_BYTES];
} q_debug_type;

typedef struct
{
    uint64_t q_save_pref;
    uint64_t q_mem_base;
    uint64_t q_len;
    char q_desc[DLOAD_DEBUG_STRLEN_BYTES];
    char q_filename[DLOAD_DEBUG_STRLEN_BYTES];
} q_debug_type_64bit;

typedef struct
{
    void* q_rx_buf;
    void* q_tx_buf;
    void* q_misc_buf;
    q_sahara_state q_state;
    size_t q_timed_data_size;
    int q_fd;
    int q_ram_dump_image;
    int q_max_ram_dump_retries;
    uint32_t q_max_ram_dump_read;
    q_sahara_mode q_mode;
    q_sahara_mode q_prev_mode;
    unsigned int q_cmd;
    bool q_ram_dump_64bit;
} q_sahara_data_t;
#endif
