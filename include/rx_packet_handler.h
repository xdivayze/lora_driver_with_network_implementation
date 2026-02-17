#pragma once
#include "packet.h"

typedef enum
{
    PACKET_REJECTED,
    COMMAND_PACKET,
    CAPTURE_BEGIN,
    CAPTURE_END,
    CAPTURE_ARRAY_FULL,
    DATA_PACKET_CAPTURED,
    CAPTURE_ALREADY_ON,
    UNKNOWN_PACKET,
    NULL_PACKET,
    CAPTURE_NOT_ON,
    PACKET_REDUNDANT,
    HANDLER_NOT_CONFIGURED,
} rx_handler_return;

typedef void (*command_packet_handler_t)(packet *p);
typedef void (*capture_end_handler_t)(packet **p_arr, int n);
void set_remote_addr(uint16_t cfg_remote_src_addr);
void configure_rx_packet_handler(command_packet_handler_t cfg_command_packet_handler, capture_end_handler_t cfg_capture_end_handler, uint16_t cfg_host_addr, uint16_t cfg_remote_src_addr);
void reset_packet_handler_state();
packet **get_rx_captured_packet_array();
rx_handler_return rx_packet_handler(packet *rx_p);