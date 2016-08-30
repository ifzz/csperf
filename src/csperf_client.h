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

#ifndef __CS_PERF_CLIENT_RUN_H
#define __CS_PERF_CLIENT_RUN_H

#include <event2/listener.h>
#include <event2/bufferevent.h>

#include "csperf_config.h"
#include "csperf_network.h"
#include "csperf_stats.h"
#include "pi_dll.h"

enum {
    CLIENT_INIT = 0,
    CLIENT_CONNECTED,
} csperf_client_states;

typedef struct csperf_client_manager_s csperf_client_manager_t;

typedef struct csperf_client_s
{
    pi_dll_t                 client_link;
    uint32_t                 client_id;
    uint8_t                  transfer_flags;
    int                      state;
    struct event             *second_timer;
    struct bufferevent       *buff_event;
    csperf_message_pdu       *data_pdu;
    csperf_message_pdu       *command_pdu_table[CS_CMD_MAX];
    csperf_client_manager_t  *cli_mgr;
    csperf_stats_t           stats;
} csperf_client_t;

struct csperf_client_manager_s
{
    FILE                  *output_file;
    struct event_base     *evbase;
    csperf_config_t       *config;
    struct event          *second_timer;
    pi_dll_t              client_free_list;
    int                   repeat_count;
    uint32_t              completed_clients_per_cycle;
    uint32_t              attempted_clients_per_cycle;
    uint32_t              active_clients_per_cycle;
    uint32_t              attempted_clients_per_second;
    csperf_global_stats_t stats;
    csperf_client_t       client_table[1];
};

int csperf_client_run();
#endif /* __CS_PERF_CLIENT_RUN_H */
