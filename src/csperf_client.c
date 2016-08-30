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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <inttypes.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include "csperf_client.h"
#include "csperf_network.h"
#include "csperf_common.h"
#include "log.h"

#define MAX_CLIENTS_TO_RUN 100 /* Maximum number of clients that we can connect to in one loop */
static int csperf_client_manager_setup_clients(csperf_client_manager_t *cli_mgr, uint32_t total_clients);
static int csperf_client_manager_timer_update(csperf_client_manager_t *cli_mgr, struct timeval *timeout);
static void csperf_client_shutdown(csperf_client_t *client, int ignore);

void
csperf_client_print_percentage_completed(csperf_client_manager_t *cli_mgr)
{
    static int percentage_factor = 1;
    double threshold = ceil((double)cli_mgr->config->total_clients / 10 * percentage_factor);

    if (cli_mgr->config->total_clients < 10) {
        return;
    }

    if (cli_mgr->completed_clients_per_cycle == threshold) {
        fprintf(stdout, "%d%% completed...\n", percentage_factor * 10);
        fflush(stdout);
        if (cli_mgr->completed_clients_per_cycle ==
                cli_mgr->config->total_clients) {
            /* Reset */
            percentage_factor = 1;
        } else {
            percentage_factor++;
        }
    }
}

/* Shutdown and cleanup the client manager */
static void
csperf_client_manager_shutdown(csperf_client_manager_t *cli_mgr)
{
    int i;
    csperf_client_t *client;

    if (!cli_mgr) {
        return;
    }
    cli_mgr->stats.end_time = csperf_network_get_time(NULL);
    csperf_output_stats(&cli_mgr->stats, cli_mgr->output_file);
    csperf_stats_printf(NULL, "Detailed test summary can be found in csperf_client_out.txt file\n");

    for (i = 0; i < cli_mgr->config->total_clients; i++) {
        client = &cli_mgr->client_table[i];
        csperf_client_shutdown(client, 1);
    }

    if (cli_mgr->output_file) {
        fclose(cli_mgr->output_file);
    }

    if (cli_mgr->second_timer) {
        event_free(cli_mgr->second_timer);
    }

    event_base_loopbreak(cli_mgr->evbase);
    event_base_free(cli_mgr->evbase);
    csperf_config_cleanup(cli_mgr->config);
    free(cli_mgr);
    zlog_info(log_get_cat(), "%s: Successfully shutdown client manager\n", __FUNCTION__);
    zlog_fini();
}

static void
csperf_client_shutdown(csperf_client_t *client, int ignore)
{
    int i;

    /* Don't do anything if the client is already freed.
     * I.E if it is queued in the free list */
    if (!client || pi_dll_queued(&client->client_link)) {
        return;
    }

    csperf_output_stats_to_file(&client->stats, client->cli_mgr->output_file);
    memset(&client->stats, 0, sizeof(client->stats));
    pi_dll_insert_tail(&client->cli_mgr->client_free_list, &client->client_link);

    if (client->buff_event) {
        bufferevent_free(client->buff_event);
        client->buff_event = NULL;
    }

    if (client->second_timer) {
        event_free(client->second_timer);
        client->second_timer = NULL;
    }

    for(i = 0; i < CS_CMD_MAX; i++) {
        if (client->command_pdu_table[i]) {
            free(client->command_pdu_table[i]);
            client->command_pdu_table[i] = NULL;
        }
    }
    if (client->data_pdu) {
        free(client->data_pdu);
        client->data_pdu = NULL;
    }
    client->state = CLIENT_INIT;
    client->cli_mgr->completed_clients_per_cycle++;
    client->cli_mgr->active_clients_per_cycle--;

    zlog_info(log_get_cat(), "%s: Client(%u):  Cleaning up connection on the client. "
            "Connections completed: %u\n",
            __FUNCTION__, client->client_id, client->cli_mgr->completed_clients_per_cycle);

    if (ignore) {
        /* Don't do any further checks */
        return;
    }

    csperf_client_print_percentage_completed(client->cli_mgr);

    /* Check if all the clients are done. */
    if (client->cli_mgr->completed_clients_per_cycle == client->cli_mgr->config->total_clients) {
        client->cli_mgr->repeat_count++;

        /* Check if we need to repeat the test */
        if (client->cli_mgr->repeat_count >= client->cli_mgr->config->repeat_count) {
            csperf_client_manager_shutdown(client->cli_mgr);
        } else {
            struct timeval timeout;

            timeout.tv_sec = 0;
            timeout.tv_usec = 1000;

            /* We need to repeat the test. Setting attempted_clients_per_cycle to 0 will allow
             * csperf_client_manager_timer_cb() to set up the clients again */
            client->cli_mgr->attempted_clients_per_cycle = 0;
            client->cli_mgr->completed_clients_per_cycle = 0;
            client->cli_mgr->active_clients_per_cycle = 0;
            client->cli_mgr->stats.end_time = csperf_network_get_time(NULL);
            csperf_client_manager_timer_update(client->cli_mgr, &timeout);
            csperf_output_stats(&client->cli_mgr->stats, client->cli_mgr->output_file);
        }
    } else if (client->cli_mgr->config->concurrent_clients &&
            client->cli_mgr->attempted_clients_per_cycle <
            client->cli_mgr->config->total_clients) {
        /* Need to start a new client */
        if ((csperf_client_manager_setup_clients(client->cli_mgr, 1))) {
            csperf_client_manager_shutdown(client->cli_mgr);
        }
    }
}

/* Update Client Manager timer. It ticks every 1 second */
static int
csperf_client_manager_timer_update(csperf_client_manager_t *cli_mgr, struct timeval *timeout)
{
    evtimer_add(cli_mgr->second_timer, timeout);
    return 0;
}

/* Update timer. It ticks every 1 second */
static int
csperf_client_timer_update(csperf_client_t *client)
{
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    evtimer_add(client->second_timer, &timeout);

    return 0;
}

/* Determine the number of clients to run now */
static uint32_t
csperf_client_manager_clients_to_run(csperf_client_manager_t *cli_mgr, struct timeval *timeout)
{
    uint32_t clients_pending, clients_to_run = 0;

    if (cli_mgr->attempted_clients_per_cycle >= cli_mgr->config->total_clients) {
        /* No new clients to run */
        return 0;
    }

    if (cli_mgr->config->clients_per_sec) {
        if (cli_mgr->attempted_clients_per_second < cli_mgr->config->clients_per_sec) {
            clients_pending = cli_mgr->config->clients_per_sec - cli_mgr->attempted_clients_per_second;
            clients_to_run = (clients_pending < MAX_CLIENTS_TO_RUN) ? clients_pending : MAX_CLIENTS_TO_RUN;
            if (clients_pending < MAX_CLIENTS_TO_RUN) {
                clients_to_run = clients_pending;
            } else {
                clients_to_run = MAX_CLIENTS_TO_RUN;
                timeout->tv_sec = 0;
                timeout->tv_usec = 1000;
            }
        } else if (cli_mgr->attempted_clients_per_second == cli_mgr->config->clients_per_sec) {
            cli_mgr->attempted_clients_per_second = 0;
            if (cli_mgr->config->clients_per_sec < MAX_CLIENTS_TO_RUN) {
                clients_to_run = cli_mgr->config->clients_per_sec;
            } else {
                clients_to_run = MAX_CLIENTS_TO_RUN;
                timeout->tv_sec = 0;
                timeout->tv_usec = 1000;
            }
        } else {
            assert(0);
        }
    } else if (cli_mgr->config->concurrent_clients) {
        assert(cli_mgr->config->concurrent_clients >= cli_mgr->active_clients_per_cycle);
        clients_pending = cli_mgr->config->concurrent_clients - cli_mgr->active_clients_per_cycle;
        if (clients_pending < MAX_CLIENTS_TO_RUN) {
            clients_to_run = clients_pending;
        } else {
            clients_to_run = MAX_CLIENTS_TO_RUN;
            timeout->tv_sec = 0;
            timeout->tv_usec = 1000;
        }
    } else {
        clients_pending = cli_mgr->config->total_clients - cli_mgr->attempted_clients_per_cycle;

        if (clients_pending < MAX_CLIENTS_TO_RUN) {
            clients_to_run = clients_pending;
        } else {
            clients_to_run = MAX_CLIENTS_TO_RUN;

            /* Schedule the timer callback sooner since there are still pending connections */
            timeout->tv_sec = 0;
            timeout->tv_usec = 1000;
        }
    }
    return clients_to_run;
}

/* Monitor how many connections are setup. */
static void
csperf_client_manager_timer_cb(int fd, short kind, void *userp)
{
    csperf_client_manager_t *cli_mgr = (csperf_client_manager_t *)userp;
    uint32_t clients_to_run;
    struct timeval timeout;

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    /* Determine if we need to setup more clients */
    if ((clients_to_run = csperf_client_manager_clients_to_run(cli_mgr, &timeout))) {
        zlog_info(log_get_cat(), "%s: Running %u clients\n",
                __FUNCTION__, clients_to_run);
        if ((csperf_client_manager_setup_clients(cli_mgr, clients_to_run))) {
            csperf_client_manager_shutdown(cli_mgr);
            return;
        }
    }
    csperf_client_manager_timer_update(cli_mgr, &timeout);
}

/* Called when the timeout happens.
 * We don't do anything useful here. yet.. */
static void
csperf_client_timer_cb(int fd, short kind, void *userp)
{
    static uint16_t timer = 0;

    csperf_client_t *client = (csperf_client_t *)userp;

    /* Check if we need to stop */
    if ((client->cli_mgr->config->client_runtime) &&
            (++timer >= client->cli_mgr->config->client_runtime)) {
        zlog_info(log_get_cat(), "%s: Client(%u): Timeout\n",
                __FUNCTION__, client->client_id);
        csperf_client_shutdown(client, 0);
        return;
    }

    /* TODO: Display current stats */
    csperf_client_timer_update(client);
}

/* Got SIGINT */
static void
csperf_client_signal_cb(evutil_socket_t sig, short events, void *user_data)
{
    fprintf(stderr, "Caught an interrupt signal. Shutting down client.\n");
    csperf_client_manager_shutdown((csperf_client_manager_t *) user_data);
}

/* Init client subsystem */
static csperf_client_manager_t *
csperf_client_manager_init(csperf_config_t *config)
{
    int i;
    csperf_client_manager_t *cli_mgr;
    csperf_client_t *client = NULL;
    uint32_t client_id_count = 0;

    cli_mgr = (csperf_client_manager_t *) calloc (1, sizeof(csperf_client_manager_t) +
            config->total_clients * sizeof(csperf_client_t));

    if (!cli_mgr) {
        return NULL;
    }

    if (!(cli_mgr->evbase = event_base_new())) {
        free(cli_mgr);
        return NULL;
    }
    cli_mgr->config = config;
    pi_dll_init(&cli_mgr->client_free_list);

    for (i = 0; i < config->total_clients; i++) {

        client = &cli_mgr->client_table[i];

        pi_dll_init(&client->client_link);
        pi_dll_insert_tail(&cli_mgr->client_free_list, &client->client_link);
        client->client_id = ++client_id_count;
        client->cli_mgr = cli_mgr;
        client->cli_mgr->config = config;
    }

    if (config->client_output_file) {
        if (!(cli_mgr->output_file = fopen(config->client_output_file, "w"))) {
            zlog_warn(log_get_cat(), "Failed to create %s file\n", config->client_output_file);
            free(client);
            return NULL;
        }
    }
    zlog_info(log_get_cat(), "%s: Initialised client manager\n", __FUNCTION__);
    return cli_mgr;
}

/* Send this command to ask the server to send back the response
 * after processing total_data_blocks. We then calculate the rtt to measure
 * the time taken to process total_data_blocks */
static int
csperf_client_send_mark_command(csperf_client_t *client, uint8_t flags)
{
    csperf_command_pdu *command;

    command = (csperf_command_pdu *)(&client->
            command_pdu_table[CS_CMD_MARK]->message);

    command->blocks_to_receive = client->cli_mgr->config->total_data_blocks;
    command->timestamp =
        csperf_network_get_time(client->stats.mark_sent_time);

    client->transfer_flags = command->flags = flags;
    client->stats.total_commands_sent++;
    client->cli_mgr->stats.total_commands_sent++;

    return bufferevent_write(client->buff_event,
        client->command_pdu_table[CS_CMD_MARK],
        CS_HEADER_PDU_LEN + CS_COMMAND_PDU_LEN);
}

/* Send data message to the server */
static int
csperf_client_send_data(csperf_client_t *client)
{
    /* Write to socket */
    int error;

    if (client->cli_mgr->config->data_size &&
        client->stats.total_bytes_sent >= client->cli_mgr->config->data_size) {
        return 0;
    }

    if (client->stats.total_blocks_sent >=
            client->cli_mgr->config->total_data_blocks) {
        return 0;
    }

#if 0
    error = write(bufferevent_getfd(client->buff_event),
            client->data_pdu, client->cli_mgr->config->data_block_size + CS_HEADER_PDU_LEN);
    fprintf(stdout, "Bytes written: %d\n", error);
#endif

    error = bufferevent_write(client->buff_event, client->data_pdu,
        client->cli_mgr->config->data_block_size + CS_HEADER_PDU_LEN);

    if (error) {
        assert(0);
        return error;
    }
    client->stats.total_bytes_sent +=  client->cli_mgr->config->data_block_size +
        CS_HEADER_PDU_LEN;
    client->stats.total_blocks_sent++;
    client->cli_mgr->stats.total_bytes_sent +=  client->cli_mgr->config->data_block_size +
        CS_HEADER_PDU_LEN;
    client->cli_mgr->stats.total_blocks_sent++;
    return 0;
}

/* Start the transfer.
 * First send mark command followed by data */
static void
csperf_client_start(csperf_client_t *client)
{
    if (csperf_client_send_mark_command(client,
        client->cli_mgr->config->transfer_mode) < 0) {
        zlog_warn(log_get_cat(), "Error writing command");
    }
    if (csperf_client_send_data(client) < 0) {
        zlog_warn(log_get_cat(), "Write error\n");
    }
}

/* Receive data. For now, just discard it */
static int
csperf_client_process_data(csperf_client_t *client, struct evbuffer *buf,
        uint32_t len)
{
    /* Should not receive data in half duplex mode */
    assert(client->transfer_flags == CS_FLAG_DUPLEX);

    /* Silent drain data */
    client->stats.total_bytes_received += len;
    client->stats.total_blocks_received++;
    client->cli_mgr->stats.total_bytes_received += len;
    client->cli_mgr->stats.total_blocks_received++;
    evbuffer_drain(buf, len);
    return 0;
}

/* Process the command  */
static int
csperf_client_process_command(csperf_client_t *client, struct evbuffer *buf)
{
    csperf_command_pdu command = { 0 };

    /* Remove header */
    evbuffer_drain(buf, CS_HEADER_PDU_LEN);

    evbuffer_remove(buf, &command, CS_COMMAND_PDU_LEN);
    client->stats.total_commands_received++;
    client->cli_mgr->stats.total_commands_received++;

    switch (command.command_type) {
    case CS_CMD_MARK_RESP:
        /* Calculate the time taken to process all the data */
        client->stats.time_to_process_data =
            csperf_network_get_time(client->stats.mark_received_time) -
            command.timestamp;

        csperf_client_shutdown(client, 0);
        break;
    default:
        zlog_info(log_get_cat(), "%s: Client(%u): Unexpected mark command\n",
                __FUNCTION__, client->client_id);
        return -1;
    }
    return 0;
}

/* Invoked when we can write data to buffer event */
static void
csperf_client_write_cb(struct bufferevent *bev, void *ptr)
{
    csperf_client_t *client = (csperf_client_t *)ptr;
    csperf_client_send_data(client);
}

/* Called when there is new stuff to be read */
static void
csperf_client_readcb(struct bufferevent *bev, void *ptr)
{
    csperf_client_t *client = (csperf_client_t *) ptr;
    struct evbuffer *input_buf;
    int message_type;
    uint32_t len = 0;

    do {
        /* Get buffer from input queue */
        input_buf = bufferevent_get_input(bev);
        message_type = csperf_network_get_pdu_type(input_buf, &len);

        /* Complete pdu hasn't arrived yet */
        if (!message_type) {
            break;
        }

        if (message_type == CS_MSG_DATA) {
            csperf_client_process_data(client, input_buf, len);
        } else if (message_type == CS_MSG_COMMAND) {
            /* We got a command from server */
            csperf_client_process_command(client, input_buf);
        } else {
            assert(0);
        }
    } while(input_buf && evbuffer_get_length(input_buf));
}

/* Called when there is an event on the socket */
static void
csperf_client_eventcb(struct bufferevent *bev, short events, void *ctx)
{
    csperf_client_t *client = ctx;
    csperf_client_manager_t *cli_mgr = client->cli_mgr;
    int finished = 0;

    /* Connected to the server */
    if (events & BEV_EVENT_CONNECTED) {
        client->state = CLIENT_CONNECTED;
        zlog_info(log_get_cat(), "%s: Client(%u): Connected to server\n",
                __FUNCTION__, client->client_id);

        /* Update the stats */
        cli_mgr->stats.total_connection_connected++;

        /* Start the transfer */
        csperf_client_start(client);
    } else {
        if (events & BEV_EVENT_EOF) {
            finished = 1;
        }
        if (events & BEV_EVENT_ERROR) {
            zlog_info(log_get_cat(), "%s: Client(%u): Socket error: %s\n",
                    __FUNCTION__, client->client_id,
                    evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
            strcpy(client->stats.error_message, evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
            if (client->state >= CLIENT_CONNECTED) {
                cli_mgr->stats.total_connection_errors++;
            } else {
                cli_mgr->stats.total_connects_failed++;
            }
            finished = 1;
        }
        if (finished) {
            csperf_client_shutdown(client, 0);
        }
    }
}

/* Set up client */
static int
csperf_client_manager_setup_clients(csperf_client_manager_t *cli_mgr, uint32_t total_clients)
{
    csperf_client_t *client;
    pi_dll_t *entry;
    int i, j;

    for (i = 0; i < total_clients; i++) {
        if (!(entry = pi_dll_dequeue_head(&cli_mgr->client_free_list))) {
            assert(0);
        }
        client = (csperf_client_t *)entry;


        /* Create data pdu. We do this once and keep sending the
         * same data over and over again */
        if (!(client->data_pdu =
                    csperf_network_create_pdu(CS_MSG_DATA, 0,
                        cli_mgr->config->data_block_size))) {
            zlog_error(log_get_cat(), "Failed to create Data pdu\n");
            fprintf(stderr, "Failed to create data pdu. Stopping test\n");
            return -1;
        }

        /* Set up all commands. We also do this once.
         * Not everything we set up might be used */
        for (j = 0; j < CS_CMD_MAX; j++) {
            if (!(client->command_pdu_table[j] =
                csperf_network_create_pdu(CS_MSG_COMMAND, j,
                    CS_COMMAND_PDU_LEN))) {
                zlog_error(log_get_cat(), "Failed to create Command pdu\n");
                fprintf(stderr, "Failed to create command pdu. Stopping test\n");
                return -1;
            }
        }

        /* Create a buffer event */
        client->buff_event = bufferevent_socket_new(cli_mgr->evbase, -1,
                BEV_OPT_CLOSE_ON_FREE);

        if (!client->buff_event) {
            zlog_error(log_get_cat(), "Failed to get a new socket\n");
            fprintf(stderr, "Unable to allocate a socket. Stopping test\n");
            return -1;
        }

        /* Set read write and event callbacks on the buffer event */
        bufferevent_setcb(client->buff_event, csperf_client_readcb,
            csperf_client_write_cb, csperf_client_eventcb, client);
        bufferevent_enable(client->buff_event, EV_READ|EV_WRITE);

        /* Set the low watermark to size of the asn header.
           That way we can figure out if it is data or command */
        bufferevent_setwatermark(client->buff_event, EV_READ, CS_HEADER_PDU_LEN, 0);

        /* Connect to the host name, This is non-blocking */
        if (bufferevent_socket_connect_hostname(client->buff_event,
                    NULL, AF_INET, client->cli_mgr->config->server_hostname,
                    client->cli_mgr->config->server_port)) {
            bufferevent_free(client->buff_event);
            zlog_error(log_get_cat(), "Connect error. Stopping test\n");
            return -1;
        }

        client->second_timer = evtimer_new(cli_mgr->evbase,
            csperf_client_timer_cb, client);
        csperf_client_timer_update(client);
        cli_mgr->attempted_clients_per_cycle++;
        cli_mgr->active_clients_per_cycle++;
        cli_mgr->attempted_clients_per_second++;
        cli_mgr->stats.total_connection_attempts++;
        zlog_info(log_get_cat(), "%s: Client(%u): Connecting to server: %s:%u"
                " Total attempts: %"PRIu64"\n",
                __FUNCTION__, client->client_id, client->cli_mgr->config->server_hostname,
                 client->cli_mgr->config->server_port, cli_mgr->stats.total_connection_attempts);
    }
    return 0;
}

int
csperf_client_run(csperf_config_t *config)
{
    csperf_client_manager_t *cli_mgr = NULL;
    struct event     *signal_event = NULL;
    struct timeval timeout;

    if (!(cli_mgr = csperf_client_manager_init(config))) {
        csperf_config_cleanup(config);
        zlog_warn(log_get_cat(), "Failed to init client\n");
        return -1;
    }

    /* Setup signal handler for SIGINT */
    signal_event = evsignal_new(cli_mgr->evbase, SIGINT,
            csperf_client_signal_cb, cli_mgr);
    if (!signal_event || ((event_add(signal_event, NULL) < 0))) {
        csperf_client_manager_shutdown(cli_mgr);
        return -1;
    }
    cli_mgr->stats.start_time = csperf_network_get_time(NULL); 

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    cli_mgr->second_timer = evtimer_new(cli_mgr->evbase,
        csperf_client_manager_timer_cb, cli_mgr);
    evtimer_add(cli_mgr->second_timer, &timeout);
    fprintf(stdout, "0%% completed...\n");
    fflush(stdout);

    /* Run the event loop. Connect to the server */
    event_base_dispatch(cli_mgr->evbase);

    return 0;
}
