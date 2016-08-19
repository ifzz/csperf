#include <stdio.h>
#include <stdlib.h>

#include "csperf_defaults.h"
#include "csperf_client.h"
#include "csperf_server.h"

/* Kickstart things */
int
csperf_main_start_test(csperf_config_t *config)
{
    if (config->role == CS_CLIENT) {
        return csperf_client_run(config);
    } else {
        return csperf_server_run(config);
    }
}

int
main(int argc, char **argv)
{
    csperf_config_t *config = NULL;
    int error;

    if (!(config = csperf_config_init())) {
        printf("Failed to init config\n");
        exit(1);
    }

    if ((error = csperf_config_parse_arguments(config, argc, argv))) {
        csperf_config_cleanup(config);
        exit(1);
    }

    if ((error = csperf_main_start_test(config))) {
        printf("Failed to start the test\n");
        exit(1);
    }
    return 0;
}
