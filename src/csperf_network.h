#ifndef __CS_PERF_NETWORK_H
#define __CS_PERF_NETWORK_H

#include <stdint.h>
#include <event2/buffer.h>

#define CS_MAGIC 0xaa
#define CS_HEADER_PDU_LEN (sizeof(asn_message_pdu)) 
#define CS_COMMAND_PDU_LEN (sizeof(asn_command_pdu)) 

/* Flags */
#define CS_FLAG_DUPLEX      0x01
#define CS_FLAG_HALF_DUPLEX 0x02

/* Each packet that gets sent out can be either
 * a data or a command */
enum asn_message_type {
    CS_MSG_COMMAND = 1,
    CS_MSG_DATA
};

/* Mark: Sent from client->server. 
         Asks the server to respond back after specifed amount
         of bytes.

   Mark response: Sent from server->client.
        After the server has done processing data
*/
enum asn_command_type {
    CS_CMD_MARK,
    CS_CMD_MARK_RESP,

    /* Place new commands above this */
    CS_CMD_MAX,
};

typedef struct 
{
    /* Header */
    uint32_t total_len;
    uint8_t  magic;
    uint8_t  message_type; 
    uint8_t  reserved[2];

    /* Payload */
    uint8_t  message[];
} asn_message_pdu;

typedef struct
{
    uint64_t       echo_timestamp;
    uint64_t       echoreply_timestamp;
    uint32_t       blocks_to_receive;
    uint8_t        command_type;
    uint8_t        flags;
    uint8_t        resvrd[2];
} asn_command_pdu;

int csperf_network_get_pdu_type(struct evbuffer *buf, uint32_t *len);
asn_message_pdu* csperf_network_create_pdu(uint8_t message_type,
         uint8_t message_info, uint32_t message_len);
uint64_t csperf_network_get_time(char *buf);
#endif /* __CS_PERF_NETWORK_H */
