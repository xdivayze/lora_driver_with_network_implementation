#include "rx_packet_handler.h"
#define PACKET_CAPTURE_MAX_COUNT (UINT8_MAX - 1)

static bool capture = false;
static uint8_t capture_ack_id = 0x00;
static packet *captured_packet_arr[PACKET_CAPTURE_MAX_COUNT] = {NULL};
static size_t captured_n = 0;

static uint16_t remote_src_addr = 0x00;
static uint16_t host_addr = 0x00;

static bool handlerConfigured = false;

command_packet_handler_t command_packet_handler;
capture_end_handler_t capture_end_handler;

void set_remote_addr(uint16_t cfg_remote_src_addr)
{
    remote_src_addr = cfg_remote_src_addr;
}
// passed callback functions must take ownership of the passed packets
void configure_rx_packet_handler(command_packet_handler_t cfg_command_packet_handler, capture_end_handler_t cfg_capture_end_handler, uint16_t cfg_host_addr, uint16_t cfg_remote_src_addr)
{
    command_packet_handler = cfg_command_packet_handler;
    capture_end_handler = cfg_capture_end_handler;
    host_addr = cfg_host_addr;
    remote_src_addr = cfg_remote_src_addr;
    handlerConfigured = true;
}

// resets the array index
// resets capture state to false
// resets capture ack id
void reset_packet_handler_state()
{
    capture = false;
    captured_n = 0;
    capture_ack_id = 0x00;
}

packet **get_rx_captured_packet_array()
{
    return captured_packet_arr;
}
// rx_p ownership transferred to callee
rx_handler_return rx_packet_handler(packet *rx_p)
{

    static uint8_t last_received_ack_id = 0x00;
    static uint8_t last_received_seq_number = 0x00;
    rx_handler_return ret = UNKNOWN_PACKET;

    if (!handlerConfigured)
    {
        ret = HANDLER_NOT_CONFIGURED;
        goto cleanup;
    }

    if (!rx_p)
    {
        ret = NULL_PACKET;
        goto cleanup;
    }

    if (rx_p->dest_address != host_addr)
    {
        ret = PACKET_REJECTED;
        goto cleanup;
    }

    if (rx_p->src_address != remote_src_addr)
    {
        ret = PACKET_REJECTED;
        goto cleanup;
    }

    // reject redundant packets
    if ((rx_p->ack_id == last_received_ack_id) && (rx_p->sequence_number == last_received_seq_number))
    {
        ret = PACKET_REDUNDANT;
        goto cleanup;
    }

    last_received_ack_id = rx_p->ack_id;
    last_received_seq_number = rx_p->sequence_number;
    switch (check_packet_type(rx_p))
    {
    case PACKET_BEGIN:
        if (!capture)
        {
            reset_packet_handler_state();
            capture = true;
            capture_ack_id = rx_p->ack_id;
            ret = CAPTURE_BEGIN;
        }
        else
            ret = CAPTURE_ALREADY_ON;

        break;

    case PACKET_DATA:
        if (capture && (rx_p->ack_id == capture_ack_id))
        { // if ack id is being captured
            if (captured_n >= PACKET_CAPTURE_MAX_COUNT)
            {
                ret = CAPTURE_ARRAY_FULL;
                break;
            }

            // reject the packet if it is not the next sequential one
            if ((rx_p->sequence_number != (captured_n + 1))) // data packets start at index 1, check for the next one
            {
                ret = PACKET_REJECTED;
                goto cleanup;
            }
            packet* rx_p_copy = copy_packet(rx_p);
            captured_packet_arr[captured_n] = rx_p_copy;
            captured_n++;
            ret = DATA_PACKET_CAPTURED;
            break;
        }
        if (rx_p->ack_id != capture_ack_id)
        { // ack id is not being captured, process command packet
            packet* rx_p_copy = copy_packet(rx_p);
            command_packet_handler(rx_p_copy);
            ret = COMMAND_PACKET;
            break;
        }
        // reject if capture is off and ack id is being captured
        ret = PACKET_REJECTED;
        break;

    case PACKET_END:
        if (!capture)
        {
            ret = CAPTURE_NOT_ON;
            break;
        }
        capture_end_handler(captured_packet_arr, captured_n);
        reset_packet_handler_state();
        ret = CAPTURE_END;
        break;

    default:
        ret = UNKNOWN_PACKET;
        break;
    }

cleanup:
    return ret;
}