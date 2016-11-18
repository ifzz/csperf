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

#ifndef __CS_PERF_NETWORK_H
#define __CS_PERF_NETWORK_H

#include <stdint.h>
#include <event2/buffer.h>

#define MAX_ALLOWED_CLIENTS 16000
#define CS_MAGIC 0xaa
#define CS_HEADER_PDU_LEN (sizeof(csperf_message_pdu)) 
#define CS_COMMAND_PDU_LEN (sizeof(csperf_command_pdu)) 

/* Flags */
#define CS_FLAG_DUPLEX      0x01
#define CS_FLAG_HALF_DUPLEX 0x02

/* Each packet that gets sent out can be either
 * a data or a command */
enum csperf_message_type {
    CS_MSG_COMMAND = 1,
    CS_MSG_DATA
};

/* Mark: Sent from client->server. 
         Asks the server to respond back after specifed amount
         of bytes.

   Mark response: Sent from server->client.
        After the server has done processing data
*/
enum csperf_command_type {
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
} csperf_message_pdu;

typedef struct
{
    uint64_t       timestamp;
    uint32_t       blocks_to_receive;
    uint32_t       time_to_run;
    uint8_t        command_type;
    uint8_t        flags;
    uint8_t        resvrd[2];
} csperf_command_pdu;

int csperf_network_get_pdu_type(struct evbuffer *buf, uint32_t *len);
csperf_message_pdu* csperf_network_create_pdu(uint8_t message_type,
         uint8_t message_info, uint32_t message_len);
uint64_t csperf_network_get_time(char *buf);
#endif /* __CS_PERF_NETWORK_H */
