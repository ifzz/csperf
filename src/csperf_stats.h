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

#ifndef __CS_PERF_STATS_H
#define __CS_PERF_STATS_H

#include <stdint.h>
typedef struct {
    uint64_t  total_bytes_sent;
    uint64_t  total_bytes_received;
    uint64_t  total_blocks_sent;
    uint64_t  total_blocks_received;
    uint64_t  total_commands_sent;
    uint64_t  total_commands_received;
    uint64_t  time_to_process_data;
    char      mark_sent_time[100];
    char      mark_received_time[100];
    char      error_message[100];
} csperf_stats_t;

typedef struct {
    uint64_t  start_time; 
    uint64_t  end_time; 
    uint64_t  total_connection_attempts;
    uint64_t  total_connection_connected;
    uint64_t  total_connects_failed;
    uint64_t  total_connection_errors;
    uint64_t  total_bytes_sent;
    uint64_t  total_bytes_received;
    uint64_t  total_blocks_sent;
    uint64_t  total_blocks_received;
    uint64_t  total_commands_sent;
    uint64_t  total_commands_received;
} csperf_global_stats_t;

void csperf_output_stats(csperf_global_stats_t *stats, FILE *fd);
void csperf_output_stats_to_file(csperf_stats_t *stats, FILE *fd);
void csperf_stats_printf(FILE *fd, const char *format, ...);
#endif /* __CS_PERF_STATS_H */
