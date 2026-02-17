#include "network_data_operations.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// consumes the packets in the buffer
int packet_array_to_data(packet **p_buf, uint8_t *data, int len)
{
    size_t size = 0;
    char *p_desc = malloc(2048);

    for (int i = 0; i < len; i++)
    {
        packet_description(p_buf[i], p_desc);
        memcpy(&data[size], p_buf[i]->payload, p_buf[i]->payload_length);
        size += p_buf[i]->payload_length;
    }

    return 0;
}

// assumes necessary bytes for callback_buf are allocated (sizeof(packet*)*npackets) and consumes data
// this function stores all packets in heap memory!! DO NOT USE FOR LARGE DATA
// number of packets can be precalculated
int data_to_packet_array(packet **callback_buf, uint8_t *data, int data_len,
                         uint16_t dest_addr, uint16_t src_addr, uint8_t ack_id,
                         bool include_handshakes)
{
    int ret = -1;
    size_t npackets = (size_t)ceil((double)data_len / payload_length_max);
    size_t data_packet_pointer_arr_size = npackets * sizeof(packet *);
    packet **buf = malloc(data_packet_pointer_arr_size);

    if (!buf)
    {
        fprintf(stderr, "no mem");
        goto cleanup;
    }
    size_t bytes_left = data_len;
    int payload_size = 0;

    int seq_offset = include_handshakes ? 1 : 0;

    for (int i = 0; i < npackets; i++)
    {
        payload_size = bytes_left < payload_length_max ? bytes_left : payload_length_max;
        uint8_t *p_data_buf = malloc(payload_size); // ownership given to packet callback buffer do not free
        memcpy(p_data_buf, data + i * payload_length_max, payload_size);
        buf[i] = packet_constructor(dest_addr, src_addr, ack_id, seq_offset + i, payload_size, p_data_buf); // sequence is offset if begin packet is included
        bytes_left -= payload_size;
    }

    size_t buf_size_w_handshake = data_packet_pointer_arr_size; // total buffer size with handshake options considered

    if (include_handshakes)
    {
        buf_size_w_handshake = data_packet_pointer_arr_size + sizeof(packet *) * 2; // adjust if handshakes are included
        buf = realloc(buf, buf_size_w_handshake);
        if (!buf)
        {
            fprintf(stderr, "no mem for reallocation\n");
            goto cleanup;
        }
        memmove(&buf[1], &buf[0], data_packet_pointer_arr_size); // reallocate and move

        buf[0] = ack_packet(dest_addr, src_addr, ack_id, 0);
        buf[npackets + 1] = ack_packet(dest_addr, src_addr, ack_id, UINT8_MAX);
    }
    memcpy(callback_buf, buf, buf_size_w_handshake);
    ret = 0;

cleanup:
    if (buf)
        free(buf);
    return ret;
}