#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>

#include "csperf_server.h"
#include "csperf_network.h"
#include "log.h"

/* Shutdown and close the client */
static void
csperf_server_shutdown(csperf_server_t *server)
{
    int i;
    csperf_client_ctx_t *cli_ctx;
    pi_dll_t *entry;

    if (!server) {
        return;
    }

    while ((entry = pi_dll_dequeue_head(&server->ctx_inuse_list))) {
        cli_ctx = (csperf_client_ctx_t *) entry;
        if (cli_ctx->show_stats) {
            ansperf_stats_display(&cli_ctx->stats, server->output_file);
        }
        if (cli_ctx->buff_event) {
            bufferevent_free(cli_ctx->buff_event);
        }
        if(cli_ctx->second_timer) {
            event_free(cli_ctx->second_timer);
        }

        for(i = 0; i < CS_CMD_MAX; i++) {
            if (cli_ctx->command_pdu_table[i]) {
                free(cli_ctx->command_pdu_table[i]);
            }
        }
    }

    if (server->output_file) {
        fclose(server->output_file);
    }
    csperf_config_cleanup(server->config);
    event_base_loopbreak(server->evbase);
    event_base_free(server->evbase);
    if (server->ctx_base) {
        free(server->ctx_base);
    }
    free(server);
    zlog_info(log_get_cat(), "%s: Successfully shutdown server\n", __FUNCTION__);
    zlog_fini();
}

/* Shutdown the client context */
static void
csperf_server_ctx_cli_shutdown(csperf_client_ctx_t *cli_ctx)
{
    int i;

    if (!cli_ctx) {
        return;
    }

    if (cli_ctx->show_stats) {
        ansperf_stats_display(&cli_ctx->stats, cli_ctx->server->output_file);
    }
    if (cli_ctx->buff_event) {
        bufferevent_free(cli_ctx->buff_event);
        cli_ctx->buff_event = NULL;
    }
    if(cli_ctx->second_timer) {
        event_free(cli_ctx->second_timer);
        cli_ctx->second_timer = NULL;
    }

    for(i = 0; i < CS_CMD_MAX; i++) {
        if (cli_ctx->command_pdu_table[i]) {
            free(cli_ctx->command_pdu_table[i]);
            cli_ctx->command_pdu_table[i] = NULL;
        }
    }
    pi_dll_unlink(&cli_ctx->ctx_link);
    pi_dll_insert_tail(&cli_ctx->server->ctx_free_list,
            &cli_ctx->ctx_link);

    zlog_info(log_get_cat(), "%s: Ctx(%"PRIu64"): Cleaning up connection on the server\n",
            __FUNCTION__, cli_ctx->ctx_id);
}

/* Display the stats first and then clear out the stats */
static void
csperf_server_reset_stats(csperf_client_ctx_t *cli_ctx)
{
    if (cli_ctx->show_stats) {
        ansperf_stats_display(&cli_ctx->stats, cli_ctx->server->output_file);

        /* Then 0 it out */
        memset(&cli_ctx->stats, 0, sizeof(cli_ctx->stats));
        cli_ctx->show_stats = 0;
    }
}

/* Update timer. It ticks every 1 second */
static int
csperf_server_timer_update(csperf_client_ctx_t *cli_ctx)
{
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    evtimer_add(cli_ctx->second_timer, &timeout);

    return 0;
}

/* Called when the timeout happens */
static void
csperf_server_timer_cb(int fd, short kind, void *userp)
{
    csperf_client_ctx_t *cli_ctx = (csperf_client_ctx_t *)userp;

    /* Display current stats */
    csperf_server_timer_update(cli_ctx);
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
    uint64_t ctx_id_counter = 0;

    server = (csperf_server_t *) calloc (1, sizeof(csperf_server_t));
    if (!server) {
        return NULL;
    }

    if (!(server->evbase = event_base_new())) {
        free(server);
        return NULL;
    }

    server->config = config;
    if (config->server_output_file) {
        if (!(server->output_file = fopen(config->server_output_file, "w"))) {
            fprintf(stderr, "Failed to create %s file\n", config->server_output_file);
            free(server);
            return NULL;
        }
    }
    pi_dll_init(&server->ctx_free_list);
    pi_dll_init(&server->ctx_inuse_list);

    csperf_client_ctx_t *cli_ctx = (csperf_client_ctx_t *) calloc(1, MAX_ALLOWED_CLIENTS * sizeof(csperf_client_ctx_t));

    if (!cli_ctx) {
        free(server);
        return NULL;
    }
    server->ctx_base = cli_ctx;

    for (i = 0; i < MAX_ALLOWED_CLIENTS; i++) {
        cli_ctx->server = server;
        cli_ctx->ctx_id = ++ctx_id_counter;
        pi_dll_init(&cli_ctx->ctx_link);
        pi_dll_insert_tail(&server->ctx_free_list, &cli_ctx->ctx_link);
        cli_ctx++;
    }

    zlog_info(log_get_cat(), "%s: Initialised server\n", __FUNCTION__);
    return server;
}

csperf_client_ctx_t*
csperf_server_get_cli_ctx(csperf_server_t *server)
{
    pi_dll_t *entry;
    csperf_client_ctx_t *cli_ctx;
    int i;

    if ((entry = pi_dll_dequeue_head(&server->ctx_free_list))) {
        cli_ctx = (csperf_client_ctx_t *) entry;

        /* Set up all commands. We also do this once.
         * Not everything we set up might be used */
        for (i = 0; i < CS_CMD_MAX; i++) {
            if (!(cli_ctx->command_pdu_table[i] =
                csperf_network_create_pdu(CS_MSG_COMMAND, i,
                    CS_COMMAND_PDU_LEN))) {
                return NULL;
            }
        }
        cli_ctx->second_timer = evtimer_new(server->evbase,
            csperf_server_timer_cb, cli_ctx);
        csperf_server_timer_update(cli_ctx);
        pi_dll_insert_tail(&server->ctx_inuse_list, &cli_ctx->ctx_link);
        return cli_ctx;
    }
    return NULL;
}

/* Called after we are done processing the client data */
static int
csperf_server_send_mark_resp_command(csperf_client_ctx_t *cli_ctx, uint8_t flags)
{
    csperf_command_pdu *command;

    command = (csperf_command_pdu *)(&cli_ctx->
            command_pdu_table[CS_CMD_MARK_RESP]->message);

    command->blocks_to_receive = cli_ctx->server->config->total_data_blocks;
    command->timestamp = 
        cli_ctx->client_last_received_timestamp;
    cli_ctx->transfer_flags = command->flags = flags;
    cli_ctx->stats.total_commands_sent++;

    /* Calculate the time to process the data */
    cli_ctx->stats.time_to_process_data =
        csperf_network_get_time(cli_ctx->stats.mark_sent_time) -
        cli_ctx->client_last_received_local_time;

    /* End of 1 cycle */
    cli_ctx->show_stats = 1;
    csperf_server_reset_stats(cli_ctx);

    return bufferevent_write(cli_ctx->buff_event,
        cli_ctx->command_pdu_table[CS_CMD_MARK_RESP],
        CS_HEADER_PDU_LEN + CS_COMMAND_PDU_LEN);
}

static int
csperf_server_process_data(csperf_client_ctx_t *cli_ctx, struct evbuffer *buf,
        uint32_t len)
{
    cli_ctx->stats.total_bytes_received += len;
    cli_ctx->stats.total_blocks_received++;

    if (cli_ctx->transfer_flags == CS_FLAG_DUPLEX) {
        /* Move it to buffer event's output queue.
         * Basically, we are just echoing back the data */
        evbuffer_remove_buffer
            (buf, bufferevent_get_output(cli_ctx->buff_event), len);
        cli_ctx->stats.total_bytes_sent +=  len;
        cli_ctx->stats.total_blocks_sent++;
    } else {
        /* Silent drain data */
        evbuffer_drain(buf, len);
    }

    /* Thats the datablocks we receive. Send mark resp command */
    if (cli_ctx->stats.total_blocks_received >=
            cli_ctx->server->config->total_data_blocks) {
        csperf_server_send_mark_resp_command(cli_ctx, 0);
    }
    return 0;
}

/* Process the command  */
static int
csperf_server_process_command(csperf_client_ctx_t *cli_ctx, struct evbuffer *buf)
{
    csperf_command_pdu command = { 0 };

    /* Remove header */
    evbuffer_drain(buf, CS_HEADER_PDU_LEN);

    evbuffer_remove(buf, &command, CS_COMMAND_PDU_LEN);

    switch (command.command_type) {
    case CS_CMD_MARK:
        assert(command.blocks_to_receive);
        cli_ctx->transfer_flags = command.flags;
        cli_ctx->server->config->total_data_blocks = command.blocks_to_receive;
        cli_ctx->client_last_received_timestamp = command.timestamp;
        cli_ctx->client_last_received_local_time =
            csperf_network_get_time(cli_ctx->stats.mark_received_time);
        break;
    default:
        zlog_warn(log_get_cat(), "%s: Ctx(%"PRIu64"): Unexpected command received: %d\n",
                __FUNCTION__, cli_ctx->ctx_id, command.command_type);
        return -1;
    }
    cli_ctx->stats.total_commands_received++;
    return 0;
}

static void
csperf_accept_error(struct evconnlistener *listener, void *ctx)
{
    int err = EVUTIL_SOCKET_ERROR();

    zlog_warn(log_get_cat(), "Server: Got an error %d (%s) on the listener. "
        "Shutting down.\n", err, evutil_socket_error_to_string(err));

    csperf_server_shutdown((csperf_server_t *)ctx);
}

/* Called when there is new stuff to be read */
void
csperf_server_readcb(struct bufferevent *bev, void *ptr)
{
    struct evbuffer *input_buf;
    int message_type;
    csperf_client_ctx_t *cli_ctx = (csperf_client_ctx_t*) ptr;
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
            csperf_server_process_data(cli_ctx, input_buf, len);
        } else if (message_type == CS_MSG_COMMAND) {
            /* We got a command from client */
            csperf_server_process_command(cli_ctx, input_buf);
        } else {
            assert(0);
        }
    } while(input_buf && evbuffer_get_length(input_buf));
}

/* Handle events that we get on a connection */
void
csperf_server_eventcb(struct bufferevent *bev, short events, void *ctx)
{
    csperf_client_ctx_t *cli_ctx = ctx;
    int finished = 0;

    if (events & BEV_EVENT_ERROR) {
        zlog_info(log_get_cat(), "%s: Ctx(%"PRIu64"): Socket error: %s\n",
            __FUNCTION__, cli_ctx->ctx_id,
             evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
        strcpy(cli_ctx->stats.error_message, evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
        finished = 1;
    }

    if (events & BEV_EVENT_EOF) {
        finished = 1;
    }

    if (finished) {
        /* Display stats */
        csperf_server_reset_stats(cli_ctx);
        csperf_server_ctx_cli_shutdown(cli_ctx);
    }
}

static void
csperf_server_accept(struct evconnlistener *listener,
    evutil_socket_t fd, struct sockaddr *address, int socklen,
    void *ctx)
{
    csperf_server_t  *server = (csperf_server_t *)ctx;
    struct event_base *base = evconnlistener_get_base(listener);
    csperf_client_ctx_t *cli_ctx = csperf_server_get_cli_ctx(server);

    if (!cli_ctx) {
        zlog_error(log_get_cat(), "%s: Unable to get client context\n", __FUNCTION__);
        fprintf(stderr, "Could not get the client context, Shutting down server\n");
        csperf_server_shutdown(server);
        return;
    }

    cli_ctx->buff_event = bufferevent_socket_new(base, fd,
            BEV_OPT_CLOSE_ON_FREE);

    /* We got a new connection! Set up a bufferevent for it. */
    /* Set callbacks */
    evutil_make_socket_nonblocking(bufferevent_getfd(cli_ctx->buff_event));
    bufferevent_setcb(cli_ctx->buff_event, csperf_server_readcb,
            NULL, csperf_server_eventcb, cli_ctx);
    bufferevent_enable(cli_ctx->buff_event, EV_READ|EV_WRITE);
    bufferevent_setwatermark(cli_ctx->buff_event, EV_READ, CS_HEADER_PDU_LEN, 0);
    cli_ctx->show_stats = 1;
    zlog_info(log_get_cat(), "%s: Ctx(%"PRIu64"): Connection is accepted\n",
            __FUNCTION__, cli_ctx->ctx_id);
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
        zlog_warn(log_get_cat(), "Failed to init server\n");
        return -1;
    }

    /* Setup signal handler for SIGINT */
    signal_event = evsignal_new(server->evbase, SIGINT,
            csperf_server_signal_cb, server);

    if ((error = csperf_server_configure(server))) {
        zlog_warn(log_get_cat(), "Failed to configure server: %s\n", strerror(errno));
        fprintf(stderr, "Failed to configure server: %s\n", strerror(errno));
        csperf_server_shutdown(server);
        return error;
    }

    if (!signal_event || ((event_add(signal_event, NULL) < 0))) {
        csperf_server_shutdown(server);
        return -1;
    }

    /* Run the event loop. Listen for connection */
    event_base_dispatch(server->evbase);
    return 0;
}
