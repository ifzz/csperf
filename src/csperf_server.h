#ifndef __CS_PERF_SERVER_H
#define __CS_PERF_SERVER_H

#include <event2/listener.h>
#include <event2/bufferevent.h>

#include "csperf_config.h"
#include "csperf_network.h"
#include "csperf_stats.h"

typedef struct csperf_server_s {
    uint8_t            transfer_flags;
    uint8_t            show_stats;
    FILE               *output_file;
    struct event_base  *evbase;
    struct bufferevent *buff_event;
    struct event       *second_timer;
    csperf_config_t    *config;
    asn_message_pdu    *command_pdu_table[CS_CMD_MAX];
    uint64_t            client_last_received_timestamp;
    csperf_stats_t      stats;
} csperf_server_t;

int csperf_server_run(csperf_config_t *config);
#endif /* __CS_PERF_SERVER_H */
