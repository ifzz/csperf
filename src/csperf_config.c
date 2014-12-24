#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <assert.h>

#include "csperf_config.h"
#include "csperf_defaults.h"
#include "csperf_network.h"

void
csperf_config_display_short_help()
{
    printf("Usage: csperf [-s|-c host] [options]\n");
    printf("Try csperf -h for more information\n");
}

void
csperf_config_display_long_help()
{
    printf("Usage: csperf [-s|-c host] [options]\n");
    printf(" -c <hostname>         # Run as client and connect to hostname\n");
    printf(" -s                    # Run as server\n");
    printf(" -p <port>             # Server port to list to. Default 5001\n");
    printf(" -B <data block size>  # Size of the data segment. Default 1KB\n");
    printf(" -n <num blcks>        # Number of data blocks to send. Default 1000\n");
    printf(" -e                    # Echo client data. Server echos client data\n");
    printf(" -r <repeat count>     # Repeat the test these many times. Setting -1 means run forever\n");
    printf(" -l <logfile>          # Logfile to write to. Default writes to csperf_xxx.txt xxx = client or server\n");
}

int
csperf_config_parse_arguments(csperf_config_t *config, 
        int argc, char **argv)
{
    int rget_opt = 0;
    if (argc < 2) {
        csperf_config_display_short_help();
        return -1;
    }

    static struct option longopts[] =
    {
        {"client", required_argument, NULL, 'c'},
        {"server", no_argument, NULL, 's'},
        {"port", required_argument, NULL, 'p'},
        {"blocksize", required_argument, NULL, 'B'},
        {"numblocks", required_argument, NULL, 'n'},
        {"echo", no_argument, NULL, 'e'},
        {"repeat", required_argument, NULL, 'r'},
        {"logfile", required_argument, NULL, 'l'},
        {"markinterval", required_argument, NULL, 'm'},
#if 0
        {"time", required_argument, NULL, 't'},
        {"bytes", required_argument, NULL, 'b'},
#endif
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    while((rget_opt = getopt_long(argc, argv, "c:sp:B:n:em:r:l:h",
                    longopts, NULL)) != -1) {
        switch (rget_opt) {
        case 'c':
            config->role = CS_CLIENT;
            config->server_hostname = strdup(optarg);
            break;
        case 's':
            config->role = CS_SERVER;
            break;
        case 'p':
            config->server_port = atoi(optarg);
            break;
        case 'B':
            config->data_block_size = atoi(optarg);
            break;
        case 'n':
            config->total_data_blocks = atoi(optarg);
            break;
        case 'e':
            config->transfer_mode = CS_FLAG_DUPLEX; 
            break;
        case 'r':
            config->repeat_count = atoi(optarg); 
            break;
        case 'm':
            config->mark_interval_percentage = atoi(optarg);
            if (config->mark_interval_percentage < 1 ||
                    config->mark_interval_percentage > 100) {
                printf("-m is mark interval percentage\n");
                return -1;
            }
            break;
        case 'l':
            if (config->role == CS_CLIENT) {
                /* Release the default value */
                free(config->client_output_file);
                config->client_output_file = strdup(optarg);
            }
            if (config->role == CS_SERVER) {
                /* Release the default value */
                free(config->server_output_file);
                config->server_output_file = strdup(optarg);
            }
            break;
#if 0
        case 't':
            config->client_runtime = atoi(optarg);
            if (config->client_runtime > MAX_CLIENT_RUNTIME) {
                printf("Exceeded max runtime\n");
                return -1;
            }
            break;
        case 'b':
            /* Use iperf's way of setting data. Look '-n' */
            config->data_size = atoi(optarg);
            break;
#endif
        case 'h':
            csperf_config_display_long_help();
            return -1;
        default:
            csperf_config_display_short_help();
            return -1;
        }
    }
    return 0;
}

void
csperf_config_set_defaults(csperf_config_t *config)
{
    config->transfer_mode = CS_FLAG_HALF_DUPLEX; 
    config->data_block_size = DEFAULT_DATA_BLOCKLEN;
    config->server_port = DEFAULT_SERVER_PORT;
    config->total_data_blocks = DEFAULT_DATA_BLOCKS;  
    config->mark_interval_percentage = 100;
    config->repeat_count = 1;

    config->client_output_file = strdup(DEFAULT_CLIENT_OUTPUT_FILE);
    config->server_output_file = strdup(DEFAULT_SERVER_OUTPUT_FILE);
}

void
csperf_config_cleanup(csperf_config_t *config)
{
    if (!config) {
        return;
    }
    if (config->server_hostname) {
        free(config->server_hostname);
    }

    if (config->client_output_file) {
        free(config->client_output_file);
    }
    if (config->server_output_file) {
        free(config->server_output_file);
    }
    free(config);
    config = NULL;
}

csperf_config_t *
csperf_config_init()
{
    csperf_config_t* config;

    config = (csperf_config_t *) calloc (1, sizeof(csperf_config_t));

    if (!config) {
        return NULL;
    }
    csperf_config_set_defaults(config);
    return config;
}
