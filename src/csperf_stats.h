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
} csperf_stats_t;

void ansperf_stats_display(csperf_stats_t *stats, FILE *fd);
#endif /* __CS_PERF_STATS_H */
