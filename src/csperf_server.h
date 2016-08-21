#ifndef __CS_PERF_SERVER_H
#define __CS_PERF_SERVER_H

#include <event2/listener.h>
#include <event2/bufferevent.h>

#include "csperf_config.h"
#include "csperf_network.h"
#include "csperf_stats.h"
#include "pi_dll.h"

typedef struct csperf_client_ctx_s csperf_client_ctx_t;

typedef struct csperf_server_s {
    FILE                *output_file;
    struct event_base   *evbase;
    csperf_config_t     *config;
    csperf_client_ctx_t *ctx_base;
    pi_dll_t            ctx_free_list;
    pi_dll_t            ctx_inuse_list;
} csperf_server_t;

struct csperf_client_ctx_s {
    pi_dll_t           ctx_link;
    uint8_t            transfer_flags;
    uint8_t            show_stats;
    struct event       *second_timer;
    struct bufferevent *buff_event;
    csperf_message_pdu *command_pdu_table[CS_CMD_MAX];
    csperf_server_t    *server;
    uint64_t           client_last_received_timestamp;
    uint64_t           ctx_id;
    csperf_stats_t     stats;
}

int csperf_server_run(csperf_config_t *config);
#endif /* __CS_PERF_SERVER_H */
