#ifndef __CS_PERF_CONFIG_H
#define __CS_PERF_CONFIG_H

#include <stdint.h>

typedef struct csperf_config_s {
    uint8_t      role; /* Client or server */
    uint8_t      transfer_mode;
    uint8_t      mark_interval_percentage; 
    uint16_t     server_port; 
    uint16_t     mark_interval; 
    uint32_t     data_block_size;   /* Block size of each data segment */
    uint32_t     total_data_blocks; /* Total blocks to be sent */
    uint16_t     client_runtime; /* Total duration of the test. -t */
    uint64_t     data_size;       /* Total size of data to send */
    int          repeat_count;
    char         *server_hostname; /* -c option */
    char         *client_output_file;
    char         *server_output_file;
}csperf_config_t;

csperf_config_t* csperf_config_init();
void csperf_config_cleanup(csperf_config_t* config);
int csperf_config_parse_arguments(csperf_config_t *config, 
        int argc, char **argv);
#endif /* __CS_PERF_CONFIG_H */
#
