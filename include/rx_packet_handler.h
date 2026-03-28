#pragma once
#include "packet.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#define PACKET_CAPTURE_MAX_COUNT (UINT8_MAX - 1)

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

typedef struct {
    command_packet_handler_t command_packet_handler;
    capture_end_handler_t capture_end_handler;
    uint16_t host_addr;
    uint16_t remote_src_addr;
    bool configured;

    bool capture;
    uint8_t capture_ack_id;
    packet *captured_packet_arr[PACKET_CAPTURE_MAX_COUNT];
    size_t captured_n;

    uint8_t last_received_ack_id;
    uint8_t last_received_seq_number;
} rx_handler_ctx_t;

void rx_handler_init(rx_handler_ctx_t *ctx, command_packet_handler_t cmd_handler, capture_end_handler_t end_handler, uint16_t host_addr, uint16_t remote_src_addr);

void rx_handler_set_remote_addr(rx_handler_ctx_t *ctx, uint16_t remote_src_addr);

void rx_handler_reset(rx_handler_ctx_t *ctx);

packet **rx_handler_get_captured_array(rx_handler_ctx_t *ctx);

rx_handler_return rx_packet_handler(rx_handler_ctx_t *ctx, packet *rx_p);
