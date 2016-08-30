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

#include <stdio.h>
#include <stdlib.h>

#include "csperf_defaults.h"
#include "csperf_client.h"
#include "csperf_server.h"
#include "log.h"

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

    /* Initalize our logging module. */
    if ((error = log_init())) {
        fprintf(stderr, "Failed to init logging module. error: %d\n", error);
        exit(1);
    }
    zlog_info(log_get_cat(), "Initialized logging module");


    if ((error = csperf_config_parse_arguments(config, argc, argv))) {
        csperf_config_cleanup(config);
        exit(1);
    }

    if ((error = csperf_main_start_test(config))) {
        exit(1);
    }
    zlog_fini();
    return 0;
}
