#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <signal.h>
#include <assert.h>

#include "csperf_server.h"
#include "csperf_network.h"

/* Shutdown and close the client */
static void
csperf_server_shutdown(csperf_server_t *server)
{
    int i;

    if (!server) {
        return;
    }

    fprintf(stdout, "Shuttdown server\n");

    if (server->show_stats) {
        ansperf_stats_display(&server->stats, server->output_file);
    }
    if (server->buff_event) {
        bufferevent_free(server->buff_event);
    }
    if(server->second_timer) {
        event_free(server->second_timer);
    }
    csperf_config_cleanup(server->config);

    if (server->command_pdu_table) {
        for(i = 0; i < CS_CMD_MAX; i++) {
            free(server->command_pdu_table[i]);
        }
    }

    if (server->output_file) {
        fclose(server->output_file);
    }
    event_base_loopbreak(server->evbase);
    event_base_free(server->evbase);
    free(server);
}

/* Display the stats first and then clear out the stats */
static void
csperf_server_reset_stats(csperf_server_t *server)
{
    if (server->show_stats) {
        ansperf_stats_display(&server->stats, server->output_file);

        /* Then 0 it out */
        memset(&server->stats, 0, sizeof(server->stats));
        server->show_stats = 0;
    }
}

/* Update timer. It ticks every 1 second */
static int
csperf_server_timer_update(csperf_server_t *server)
{
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    evtimer_add(server->second_timer, &timeout);

    return 0;
}

/* Called when the timeout happens */
static void
csperf_server_timer_cb(int fd, short kind, void *userp)
{
    csperf_server_t *server = (csperf_server_t *)userp;

    /* Display current stats */
    csperf_server_timer_update(server);
}

/* Got SIGINT */
static void
csperf_server_signal_cb(evutil_socket_t sig, short events, void *user_data)
{
    fprintf(stderr, "Caught an interrupt signal. Shutting down server\n");
    csperf_server_shutdown((csperf_server_t *)user_data);
}

/* Init server subsystem */
static csperf_server_t*
csperf_server_init(csperf_config_t *config)
{
    int i;
    csperf_server_t *server;

    server = (csperf_server_t *) calloc (1, sizeof(csperf_server_t));
    if (!server) {
        return NULL;
    }

    if (!(server->evbase = event_base_new())) {
        free(server);
        return NULL;
    }

    /* Set up all commands. We also do this once.
     * Not everything we set up might be used */
    for (i = 0; i < CS_CMD_MAX; i++) {
        if (!(server->command_pdu_table[i] =
            csperf_network_create_pdu(CS_MSG_COMMAND, i,
                CS_COMMAND_PDU_LEN))) {
            free(server);
            return NULL;
        }
    }

    if (config->server_output_file) {
        if (!(server->output_file = fopen(config->server_output_file, "w"))) {
            fprintf(stderr, "Failed to create %s file\n", config->server_output_file);
            free(server);
            return NULL;
        }
    }

    server->config = config;
    return server;
}

/* Called after we are done processing the client data */
static int
csperf_server_send_mark_resp_command(csperf_server_t *server, uint8_t flags)
{
    asn_command_pdu *command;

    command = (asn_command_pdu *)(&server->
            command_pdu_table[CS_CMD_MARK_RESP]->message);

    command->blocks_to_receive = server->config->total_data_blocks;
    command->echo_timestamp = command->echoreply_timestamp =
        server->client_last_received_timestamp;
    server->transfer_flags = command->flags = flags;
    server->stats.total_commands_sent++;

    /* Calculate the time to process the data */
    server->stats.time_to_process_data =
        csperf_network_get_time(server->stats.mark_sent_time) -
        server->client_last_received_timestamp;

    /* End of 1 cycle */
    server->show_stats = 1;
    csperf_server_reset_stats(server);

    return bufferevent_write(server->buff_event,
        server->command_pdu_table[CS_CMD_MARK_RESP],
        CS_HEADER_PDU_LEN + CS_COMMAND_PDU_LEN);
}

static int
csperf_server_process_data(csperf_server_t *server, struct evbuffer *buf,
        uint32_t len)
{
    server->stats.total_bytes_received += len;
    server->stats.total_blocks_received++;

    if (server->transfer_flags == CS_FLAG_DUPLEX) {
        /* Move it to buffer event's output queue.
         * Basically, we are just echoing back the data */
        evbuffer_remove_buffer
            (buf, bufferevent_get_output(server->buff_event), len);
        server->stats.total_bytes_sent +=  len;
        server->stats.total_blocks_sent++;
    } else {
        /* Silent drain data */
        evbuffer_drain(buf, len);
    }

    /* Thats the datablocks we receive. Send mark resp command */
    if (server->stats.total_blocks_received >=
            server->config->total_data_blocks) {
        csperf_server_send_mark_resp_command(server, 0);
    }
    return 0;
}

/* Process the command  */
static int
csperf_server_process_command(csperf_server_t *server, struct evbuffer *buf)
{
    asn_command_pdu command = { 0 };

    /* Remove header */
    evbuffer_drain(buf, CS_HEADER_PDU_LEN);

    evbuffer_remove(buf, &command, CS_COMMAND_PDU_LEN);

    switch (command.command_type) {
    case CS_CMD_MARK:
        assert(command.blocks_to_receive);
        server->transfer_flags = command.flags;
        server->config->total_data_blocks = command.blocks_to_receive;
        server->client_last_received_timestamp = command.echo_timestamp;
        csperf_network_get_time(server->stats.mark_received_time);
        break;
    default:
        fprintf(stderr, "Unexpected command\n");
        return -1;
    }
    server->stats.total_commands_received++;
    return 0;
}

static void
csperf_accept_error(struct evconnlistener *listener, void *ctx)
{
    int err = EVUTIL_SOCKET_ERROR();

    fprintf(stderr, "Server:Got an error %d (%s) on the listener. "
        "Shutting down.\n", err, evutil_socket_error_to_string(err));

    csperf_server_shutdown((csperf_server_t *)ctx);
}

/* Called when there is new stuff to be read */
void
csperf_server_readcb(struct bufferevent *bev, void *ptr)
{
    struct evbuffer *input_buf;
    int message_type;
    csperf_server_t *server = (csperf_server_t *) ptr;
    uint32_t len = 0;

    /* Get buffer from input queue */
    do {
        input_buf = bufferevent_get_input(bev);
        message_type = csperf_network_get_pdu_type(input_buf, &len);

        /* Complete pdu hasn't arrived yet */
        if (!message_type) {
            break;
        }

        if (message_type == CS_MSG_DATA) {
            csperf_server_process_data(server, input_buf, len);
        } else if (message_type == CS_MSG_COMMAND) {
            /* We got a command from server */
            csperf_server_process_command(server, input_buf);
        } else {
            assert(0);
        }
    } while(input_buf && evbuffer_get_length(input_buf));
}

/* Handle events that we get on a connection */
void
csperf_server_eventcb(struct bufferevent *bev, short events, void *ctx)
{
    csperf_server_t *server = ctx;
    int finished = 0;

    if (events & BEV_EVENT_ERROR) {
        fprintf(stderr, "Error: %s\n", evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
        finished = 1;
    }

    if (events & BEV_EVENT_EOF) {
        finished = 1;
    }

    if (finished) {
        /* Display stats */
        csperf_server_reset_stats(server);
    }
}

static void
csperf_server_accept(struct evconnlistener *listener,
    evutil_socket_t fd, struct sockaddr *address, int socklen,
    void *ctx)
{
    csperf_server_t  *server = (csperf_server_t *)ctx;
    struct event_base *base = evconnlistener_get_base(listener);

    /* Currently we can handle just one client. Create an array
       of buffer events when we need to support more */
    server->buff_event = bufferevent_socket_new(base, fd,
            BEV_OPT_CLOSE_ON_FREE);

    /* We got a new connection! Set up a bufferevent for it. */
    /* Set callbacks */
    evutil_make_socket_nonblocking(bufferevent_getfd(server->buff_event));
    bufferevent_setcb(server->buff_event, csperf_server_readcb,
            NULL, csperf_server_eventcb, server);
    bufferevent_enable(server->buff_event, EV_READ|EV_WRITE);
    bufferevent_setwatermark(server->buff_event, EV_READ, CS_HEADER_PDU_LEN, 0);
    server->show_stats = 1;
}

int
csperf_server_configure(csperf_server_t *server)
{
    struct sockaddr_in sin;
    struct evconnlistener *listener;

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(0);
    sin.sin_port = htons(server->config->server_port);

    listener = evconnlistener_new_bind(server->evbase,
        csperf_server_accept, server,
        LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
        (struct sockaddr*)&sin, sizeof(sin));

    if (!listener) {
        return -1;
    }
    evutil_make_socket_nonblocking(evconnlistener_get_fd(listener));
    evconnlistener_set_error_cb(listener, csperf_accept_error);
    fprintf(stdout, "Server: Listening on %s:%d\n",
            inet_ntoa(sin.sin_addr), server->config->server_port);
    return 0;
}

int
csperf_server_run(csperf_config_t *config)
{
    int error = 0;
    csperf_server_t *server = NULL;
    struct event     *signal_event = NULL;

    if (!(server = csperf_server_init(config))) {
        csperf_config_cleanup(config);
        fprintf(stderr, "Failed to init server\n");
        return -1;
    }

    /* Setup signal handler for SIGINT */
    signal_event = evsignal_new(server->evbase, SIGINT,
            csperf_server_signal_cb, server);

    if (!signal_event || ((event_add(signal_event, NULL) < 0))) {
        csperf_server_shutdown(server);
        return -1;
    }

    if ((error = csperf_server_configure(server))) {
        fprintf(stderr, "Failed to configure server\n");
        csperf_server_shutdown(server);
        return error;
    }

    server->second_timer = evtimer_new(server->evbase,
        csperf_server_timer_cb, server);
    csperf_server_timer_update(server);

    /* Run the event loop. Listen for connection */
    event_base_dispatch(server->evbase);
    return 0;
}
