#ifndef __CS_PERF_DEFAULTS_H
#define __CS_PERF_DEFAULTS_H

enum csperf_role {
    CS_CLIENT,
    CS_SERVER
};

/* General constants */
#define DEFAULT_DATA_BLOCKLEN (1024) 
#define DEFAULT_SERVER_PORT   5001
#define DEFAULT_DATA_BLOCKS   1
#define DEFAULT_CLIENT_OUTPUT_FILE "csperf_client_out.txt" 
#define DEFAULT_SERVER_OUTPUT_FILE "csperf_server_out.txt" 

/* Max constants */
#define MAX_CLIENT_RUNTIME 6000 

#endif /* __CS_PERF_DEFAULTS_H */
