/*
*    Copyright (C) 2016 Nikhil AP
*
*    This program is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __CS_PERF_CONFIG_H
#define __CS_PERF_CONFIG_H

#include <stdint.h>

typedef struct csperf_config_s {
    uint8_t      role; /* Client or server */
    uint8_t      transfer_mode;
    uint8_t      mark_interval_percentage;
    uint16_t     server_port;
    uint16_t     mark_interval;
    uint32_t     client_runtime;    /* Total duration of the test.*/
    uint32_t     data_block_size;   /* Block size of each data segment */
    int32_t      total_data_blocks; /* Total blocks to be sent */
    uint32_t     total_clients;     /* Total number of clients that need to connect to the server */
    uint32_t     clients_per_sec;   /* Clients that needs to connect to the server every second */
    uint32_t     concurrent_clients;/* Concurrent clients that needs to be connected to the server */
    uint64_t     data_size;         /* Total size of data to send */
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
