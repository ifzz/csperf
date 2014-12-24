#ifndef __CS_PERF_CLIENT_RUN_H
#define __CS_PERF_CLIENT_RUN_H

#include <event2/listener.h>
#include <event2/bufferevent.h>

#include "csperf_config.h"
#include "csperf_network.h"
#include "csperf_stats.h"

enum {
    CLIENT_INIT = 0,
    CLIENT_CONNECTED,
} csperf_client_states;

typedef struct csperf_client_s 
{
    uint8_t            transfer_flags; 
    int                state;
    int                repeat_count;
    FILE               *output_file;
    struct event_base  *evbase;
    struct event       *second_timer;
    struct bufferevent *buff_event;
    asn_message_pdu    *data_pdu;
    csperf_config_t   *config;
    asn_message_pdu    *command_pdu_table[CS_CMD_MAX];
    csperf_stats_t    stats;
}csperf_client_t;

int csperf_client_run();
#endif /* __CS_PERF_CLIENT_RUN_H */
