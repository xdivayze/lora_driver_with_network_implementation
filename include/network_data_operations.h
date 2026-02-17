#pragma once

#include "packet.h"
#include <stdbool.h>

int data_to_packet_array(packet **callback_buf, uint8_t *data, int data_len,
                         uint16_t dest_addr, uint16_t src_addr, uint8_t ack_id,
                         bool include_handshakes);

// handshakes not included
//  consumes the packets in the buffer
int packet_array_to_data(packet **p_buf, uint8_t *data, int len);