#include "rx_packet_handler.h"
#include <string.h>

void rx_handler_init(rx_handler_ctx_t *ctx, command_packet_handler_t cmd_handler, capture_end_handler_t end_handler, uint16_t host_addr, uint16_t remote_src_addr)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->command_packet_handler = cmd_handler;
    ctx->capture_end_handler = end_handler;
    ctx->host_addr = host_addr;
    ctx->remote_src_addr = remote_src_addr;
    ctx->configured = true;
}

void rx_handler_set_remote_addr(rx_handler_ctx_t *ctx, uint16_t remote_src_addr)
{
    ctx->remote_src_addr = remote_src_addr;
}

// resets capture state, array index, and capture ack id
void rx_handler_reset(rx_handler_ctx_t *ctx)
{
    ctx->capture = false;
    ctx->captured_n = 0;
    ctx->capture_ack_id = 0x00;
}

packet **rx_handler_get_captured_array(rx_handler_ctx_t *ctx)
{
    return ctx->captured_packet_arr;
}

// rx_p ownership transferred to callee
rx_handler_return rx_packet_handler(rx_handler_ctx_t *ctx, packet *rx_p)
{
    rx_handler_return ret = UNKNOWN_PACKET;

    if (!ctx->configured)
    {
        ret = HANDLER_NOT_CONFIGURED;
        goto cleanup;
    }

    if (!rx_p)
    {
        ret = NULL_PACKET;
        goto cleanup;
    }

    if (rx_p->dest_address != ctx->host_addr)
    {
        ret = PACKET_REJECTED;
        goto cleanup;
    }

    if (rx_p->src_address != ctx->remote_src_addr)
    {
        ret = PACKET_REJECTED;
        goto cleanup;
    }

    // reject redundant packets
    if ((rx_p->ack_id == ctx->last_received_ack_id) && (rx_p->sequence_number == ctx->last_received_seq_number))
    {
        ret = PACKET_REDUNDANT;
        goto cleanup;
    }

    ctx->last_received_ack_id = rx_p->ack_id;
    ctx->last_received_seq_number = rx_p->sequence_number;

    switch (check_packet_type(rx_p))
    {
    case PACKET_BEGIN:
        if (!ctx->capture)
        {
            rx_handler_reset(ctx);
            ctx->capture = true;
            ctx->capture_ack_id = rx_p->ack_id;
            ret = CAPTURE_BEGIN;
        }
        else
            ret = CAPTURE_ALREADY_ON;
        break;

    case PACKET_DATA:
        if (ctx->capture && (rx_p->ack_id == ctx->capture_ack_id))
        {
            if (ctx->captured_n >= PACKET_CAPTURE_MAX_COUNT)
            {
                ret = CAPTURE_ARRAY_FULL;
                break;
            }
            // reject the packet if it is not the next sequential one
            if (rx_p->sequence_number != (ctx->captured_n + 1))
            {
                ret = PACKET_REJECTED;
                goto cleanup;
            }
            packet *rx_p_copy = copy_packet(rx_p);
            ctx->captured_packet_arr[ctx->captured_n] = rx_p_copy;
            ctx->captured_n++;
            ret = DATA_PACKET_CAPTURED;
            break;
        }
        if (rx_p->ack_id != ctx->capture_ack_id)
        {
            packet *rx_p_copy = copy_packet(rx_p);
            ctx->command_packet_handler(rx_p_copy);
            ret = COMMAND_PACKET;
            break;
        }
        ret = PACKET_REJECTED;
        break;

    case PACKET_END:
        if (!ctx->capture)
        {
            ret = CAPTURE_NOT_ON;
            break;
        }
        ctx->capture_end_handler(ctx->captured_packet_arr, ctx->captured_n);
        rx_handler_reset(ctx);
        ret = CAPTURE_END;
        break;

    default:
        ret = UNKNOWN_PACKET;
        break;
    }

cleanup:
    return ret;
}
