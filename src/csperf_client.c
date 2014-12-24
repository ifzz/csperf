#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h> 
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include "csperf_client.h"
#include "csperf_network.h"
#include "csperf_common.h"

/* Shutdown and close the client */
static void
csperf_client_shutdown(csperf_client_t *client)
{
    int i;

    if (!client) {
        return;
    }

    /* Print stats only if the client is connected to the server */
    if (client->state >= CLIENT_CONNECTED) {
        ansperf_stats_display(&client->stats, client->output_file);
    }

    if (client->buff_event) {
        bufferevent_free(client->buff_event);
    }

    if (client->second_timer) {
        event_free(client->second_timer);
    }
    csperf_config_cleanup(client->config);

    if (client->command_pdu_table) {
        for(i = 0; i < CS_CMD_MAX; i++) {
            free(client->command_pdu_table[i]);
        }
    }

    if (client->data_pdu) {
        free(client->data_pdu);
    }

    if (client->output_file) {
        fclose(client->output_file);
    }
    event_base_loopbreak(client->evbase);
    event_base_free(client->evbase);
    free(client);
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

/* Called when the timeout happens.
 * We don't do anything useful here. yet.. */
static void
csperf_client_timer_cb(int fd, short kind, void *userp)
{
    static uint16_t timer = 0;

    csperf_client_t *client = (csperf_client_t *)userp;

    /* Check if we need to stop */
    if ((client->config->client_runtime) && 
            (++timer >= client->config->client_runtime)) {
        fprintf(stdout, "Client: Timeout!\n");
        csperf_client_shutdown(client);
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
    csperf_client_shutdown((csperf_client_t *) user_data);
}

/* Init client subsystem */
static csperf_client_t*
csperf_client_init(csperf_config_t *config)
{
    int i;
    csperf_client_t *client;

    client = (csperf_client_t *) calloc (1, sizeof(csperf_client_t));
    if (!client) {
        return NULL;
    }

    if (!(client->evbase = event_base_new())) {
        free(client);
        return NULL;
    }

    /* Create data pdu. We do this once and keep sending the 
     * same data over and over again */
    if (!(client->data_pdu = 
                csperf_network_create_pdu(CS_MSG_DATA, 0, 
                    config->data_block_size))) {
        free(client);
        return NULL;
    }

    /* Set up all commands. We also do this once. 
     * Not everything we set up might be used */
    for (i = 0; i < CS_CMD_MAX; i++) {
        if (!(client->command_pdu_table[i] = 
            csperf_network_create_pdu(CS_MSG_COMMAND, i, 
                CS_COMMAND_PDU_LEN))) {
            free(client);
            return NULL;
        }
    }

    if (config->client_output_file) {
        if (!(client->output_file = fopen(config->client_output_file, "w"))) {
            fprintf(stderr, "Failed to create %s file\n", config->client_output_file);
            free(client);
            return NULL;
        }
    }
    client->repeat_count = 1;
    client->config = config;
    return client;
}

/* Send this command to ask the server to send back the response 
 * after processing total_data_blocks. We then calculate the rtt to measure
 * the time taken to process total_data_blocks */
static int
csperf_client_send_mark_command(csperf_client_t *client, uint8_t flags)
{
    asn_command_pdu *command;

    command = (asn_command_pdu *)(&client->
            command_pdu_table[CS_CMD_MARK]->message);

    command->blocks_to_receive = client->config->total_data_blocks;
    command->echo_timestamp = csperf_network_get_time(
            client->stats.mark_sent_time); 

    client->transfer_flags = command->flags = flags;
    client->stats.total_commands_sent++;

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

    if (client->config->data_size && 
        client->stats.total_bytes_sent >= client->config->data_size) {
        return 0;
    }

    if (client->stats.total_blocks_sent >=
            client->config->total_data_blocks) {
        return 0;
    }

#if 0
    error = write(bufferevent_getfd(client->buff_event),
            client->data_pdu, client->config->data_block_size + CS_HEADER_PDU_LEN);
    fprintf(stdout, "Bytes written: %d\n", error);
#endif

    error = bufferevent_write(client->buff_event, client->data_pdu, 
        client->config->data_block_size + CS_HEADER_PDU_LEN);

    if (error) {
        assert(0);
        return error;
    }
    client->stats.total_bytes_sent +=  client->config->data_block_size +
        CS_HEADER_PDU_LEN;
    client->stats.total_blocks_sent++;
    return 0;
}

/* Start the transfer.
 * First send mark command followed by data */
static void
csperf_client_start(csperf_client_t *client)
{
    if (csperf_client_send_mark_command(client, 
        client->config->transfer_mode) < 0) {
        fprintf(stderr, "Error writing command");
    } 
    if (csperf_client_send_data(client) < 0) {
        fprintf(stderr, "Write error\n");
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
    evbuffer_drain(buf, len);
    return 0;
}

/* Process the command  */
static int
csperf_client_process_command(csperf_client_t *client, struct evbuffer *buf)
{
    asn_command_pdu command = { 0 };

    /* Remove header */
    evbuffer_drain(buf, CS_HEADER_PDU_LEN);

    evbuffer_remove(buf, &command, CS_COMMAND_PDU_LEN);
    client->stats.total_commands_received++;

    switch (command.command_type) {
    case CS_CMD_MARK_RESP:
        /* Calculate the time taken to process all the data */ 
        client->stats.time_to_process_data = 
            csperf_network_get_time(client->stats.mark_received_time) - 
            command.echoreply_timestamp;

        if (client->config->repeat_count > 0 && 
                client->repeat_count >= client->config->repeat_count) {
            csperf_client_shutdown(client);
        } else {
            /* Comes here when -r option is used. Run the test again */
            client->repeat_count++;
            ansperf_stats_display(&client->stats, client->output_file);
            memset(&client->stats, 0, sizeof(client->stats));
            /* start again */
            csperf_client_start(client);
        }
        break;
    default:
        fprintf(stderr, "Unexpected command\n");
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
    int finished = 0;

    /* Connected to the server */
    if (events & BEV_EVENT_CONNECTED) {
        client->state = CLIENT_CONNECTED;
        fprintf(stdout, "Client connected to server..Please wait\n");

        /* Start the transfer */
        csperf_client_start(client);
    } else {
        if (events & BEV_EVENT_EOF) {
            finished = 1;
        }
        if (events & BEV_EVENT_ERROR) {
            fprintf(stderr, "Error: %s\n", evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
            finished = 1;
        }
        if (finished) {
            csperf_client_shutdown(client);
        }
    }
}

/* Set up client */
static int
csperf_client_configure(csperf_client_t *client)
{
    /* Create a buffer event */
    client->buff_event = bufferevent_socket_new(client->evbase, -1,
            BEV_OPT_CLOSE_ON_FREE);

    /* Set read write and event callbacks on the buffer event */ 
    bufferevent_setcb(client->buff_event, csperf_client_readcb, 
        csperf_client_write_cb, csperf_client_eventcb, client);
    bufferevent_enable(client->buff_event, EV_READ|EV_WRITE);

    /* Set the low watermark to size of the asn header. 
       That way we can figure out if it is data or command */
    bufferevent_setwatermark(client->buff_event, EV_READ, CS_HEADER_PDU_LEN, 0);

    /* Connect to the host name */ 
    if (bufferevent_socket_connect_hostname(client->buff_event,
                NULL, AF_INET, client->config->server_hostname,
                client->config->server_port)) {
       bufferevent_free(client->buff_event);
       return -1;
    }
    fprintf(stdout, "Client: Connecting to: %s:%d\n", 
        client->config->server_hostname, client->config->server_port);
    return 0;
}

int 
csperf_client_run(csperf_config_t *config)
{
    int error = 0;
    csperf_client_t *client = NULL;
    struct event     *signal_event = NULL;

    if (!(client = csperf_client_init(config))) {
        fprintf(stderr, "Failed to init client\n");
        return -1;
    }

    if ((error = csperf_client_configure(client))) {
        csperf_client_shutdown(client);
        return error;
    }

    /* Setup signal handler for SIGINT */ 
    signal_event = evsignal_new(client->evbase, SIGINT, 
            csperf_client_signal_cb, client);
    if (!signal_event || ((event_add(signal_event, NULL) < 0))) {
        csperf_client_shutdown(client);
        return -1;
    }

    client->second_timer = evtimer_new(client->evbase, 
        csperf_client_timer_cb, client);
    csperf_client_timer_update(client);

    /* Run the event loop. Connect to the server */
    event_base_dispatch(client->evbase);

    return 0;
}
