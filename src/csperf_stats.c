#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>

#include "csperf_stats.h"
#include "csperf_common.h"

const char header[] =
"Cycle    Bytes_sent    Bytes_Received    Blocks_sent    Blocks_received    "
"Time_to_process(ms)    Mark_sent_time    Mark_received_time        Error\n";

const char seperator_line[] =
"------------------------------------------------------------------------"
"---------------------------------------------------------------------------\n";

void
csperf_stats_printf(FILE *fd, const char *format, ...)
{
    /* Write to file */
    va_list args;

    va_start(args, format);
    if (fd) {
        vfprintf(fd, format, args);
    }
    va_end(args);

    /* Write to stdout */
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
}

void
ansperf_stats_display(csperf_stats_t *stats, FILE *fd)
{
    static int header_displayed = 0;
    static int cycle = 0;
    char total_bytes_sent_str[50];
    char total_bytes_recv_str[50];

    if (!stats) {
        return;
    }

    if (!header_displayed) {
        csperf_stats_printf(fd, "%s%s", header, seperator_line);
        header_displayed = 1;
    }

    csperf_common_calculate_size(total_bytes_sent_str,
            stats->total_bytes_sent);
    csperf_common_calculate_size(total_bytes_recv_str,
            stats->total_bytes_received);

    csperf_stats_printf(fd, "%3d   %15s    %10s    %10"PRIu64"    %10"PRIu64"    %10"PRIu64"       %10s    "
            "%10s       %10s\n\n", ++cycle,
            total_bytes_sent_str, total_bytes_recv_str,
            stats->total_blocks_sent, stats->total_blocks_received,
            stats->time_to_process_data,
            strlen(stats->mark_received_time) ? stats->mark_sent_time : "-", 
            strlen(stats->mark_received_time) ? stats->mark_received_time : "-",
            strlen(stats->error_message) ? stats->error_message : "");
}
