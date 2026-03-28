#include <assert.h>
#include "network_data_operations.h"
#include "rx_packet_handler.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define TEST_STRING       "amumumumu sejuani"
#define COMMAND_PAYLOAD_STR "rammus"

#define DADDR 0xFF00
#define SADDR 0x00FF

// --- shared callback state ---

static int end_callback_fired = 0;
static int command_callback_fired = 0;
static char reassembled_data[256] = {0};

static void reset_callback_state()
{
    end_callback_fired = 0;
    command_callback_fired = 0;
    reassembled_data[0] = '\0';
}

// --- callbacks ---

static void command_callback(packet *rx_p)
{
    assert(strcmp((char *)rx_p->payload, COMMAND_PAYLOAD_STR) == 0);
    command_callback_fired++;
    free_packet(rx_p);
}

static void noop_command_callback(packet *rx_p)
{
    command_callback_fired++;
    free_packet(rx_p);
}

static void capture_end_callback(packet **p_arr, int n)
{
    packet_array_to_data(p_arr, (uint8_t *)reassembled_data, n);
    end_callback_fired++;
}

static void noop_end_callback(packet **p_arr, int n)
{
    end_callback_fired++;
}

// --- helpers ---

// Construct a BEGIN packet (seq=0, payload_len=0)
static packet make_begin(uint16_t dest, uint16_t src, uint8_t ack_id)
{
    return (packet){.dest_address = dest, .src_address = src,
                    .ack_id = ack_id, .sequence_number = 0,
                    .payload_length = 0, .payload = NULL};
}

// Construct an END packet (seq=0xFF, payload_len=0)
static packet make_end(uint16_t dest, uint16_t src, uint8_t ack_id)
{
    return (packet){.dest_address = dest, .src_address = src,
                    .ack_id = ack_id, .sequence_number = 0xFF,
                    .payload_length = 0, .payload = NULL};
}

// Construct a DATA packet
static packet make_data(uint16_t dest, uint16_t src, uint8_t ack_id, uint8_t seq, uint8_t *payload, uint8_t payload_len)
{
    return (packet){.dest_address = dest, .src_address = src,
                    .ack_id = ack_id, .sequence_number = seq,
                    .payload_length = payload_len, .payload = payload};
}

// =====================================================================
// Edge-case tests
// =====================================================================

void test_handler_not_configured()
{
    rx_handler_ctx_t ctx = {0};
    packet p = make_begin(DADDR, SADDR, 0x01);
    assert(rx_packet_handler(&ctx, &p) == HANDLER_NOT_CONFIGURED);
}

void test_null_packet_returns_null_packet()
{
    rx_handler_ctx_t ctx;
    rx_handler_init(&ctx, noop_command_callback, noop_end_callback, DADDR, SADDR);
    assert(rx_packet_handler(&ctx, NULL) == NULL_PACKET);
}

void test_wrong_dest_rejected()
{
    rx_handler_ctx_t ctx;
    rx_handler_init(&ctx, noop_command_callback, noop_end_callback, DADDR, SADDR);
    packet p = make_begin(0x1234, SADDR, 0x01);
    assert(rx_packet_handler(&ctx, &p) == PACKET_REJECTED);
}

void test_wrong_src_rejected()
{
    rx_handler_ctx_t ctx;
    rx_handler_init(&ctx, noop_command_callback, noop_end_callback, DADDR, SADDR);
    packet p = make_begin(DADDR, 0x1234, 0x01);
    assert(rx_packet_handler(&ctx, &p) == PACKET_REJECTED);
}

void test_redundant_packet_rejected()
{
    rx_handler_ctx_t ctx;
    rx_handler_init(&ctx, noop_command_callback, noop_end_callback, DADDR, SADDR);
    packet p = make_begin(DADDR, SADDR, 0x01);
    assert(rx_packet_handler(&ctx, &p) == CAPTURE_BEGIN);
    // exact same ack_id + seq — redundant
    assert(rx_packet_handler(&ctx, &p) == PACKET_REDUNDANT);
}

void test_capture_already_on()
{
    rx_handler_ctx_t ctx;
    rx_handler_init(&ctx, noop_command_callback, noop_end_callback, DADDR, SADDR);
    packet begin1 = make_begin(DADDR, SADDR, 0x01);
    packet begin2 = make_begin(DADDR, SADDR, 0x02); // different ack_id avoids redundancy check
    assert(rx_packet_handler(&ctx, &begin1) == CAPTURE_BEGIN);
    assert(rx_packet_handler(&ctx, &begin2) == CAPTURE_ALREADY_ON);
}

void test_capture_not_on_end_without_begin()
{
    rx_handler_ctx_t ctx;
    rx_handler_init(&ctx, noop_command_callback, noop_end_callback, DADDR, SADDR);
    packet end_p = make_end(DADDR, SADDR, 0x01);
    assert(rx_packet_handler(&ctx, &end_p) == CAPTURE_NOT_ON);
}

void test_out_of_order_data_rejected()
{
    rx_handler_ctx_t ctx;
    rx_handler_init(&ctx, noop_command_callback, noop_end_callback, DADDR, SADDR);

    packet begin = make_begin(DADDR, SADDR, 0x01);
    assert(rx_packet_handler(&ctx, &begin) == CAPTURE_BEGIN);

    uint8_t payload[] = {0xAA, 0xBB};
    // seq=2 skips seq=1 — should be rejected
    packet data = make_data(DADDR, SADDR, 0x01, 2, payload, sizeof(payload));
    assert(rx_packet_handler(&ctx, &data) == PACKET_REJECTED);
}

void test_in_order_data_captured()
{
    rx_handler_ctx_t ctx;
    rx_handler_init(&ctx, noop_command_callback, noop_end_callback, DADDR, SADDR);

    packet begin = make_begin(DADDR, SADDR, 0x01);
    assert(rx_packet_handler(&ctx, &begin) == CAPTURE_BEGIN);

    uint8_t payload[] = {0xAA, 0xBB};
    packet data = make_data(DADDR, SADDR, 0x01, 1, payload, sizeof(payload));
    assert(rx_packet_handler(&ctx, &data) == DATA_PACKET_CAPTURED);
}

void test_handler_reset_clears_capture()
{
    rx_handler_ctx_t ctx;
    rx_handler_init(&ctx, noop_command_callback, noop_end_callback, DADDR, SADDR);

    packet begin = make_begin(DADDR, SADDR, 0x01);
    assert(rx_packet_handler(&ctx, &begin) == CAPTURE_BEGIN);

    rx_handler_reset(&ctx);

    // after reset, capture is off — END should return CAPTURE_NOT_ON
    packet end_p = make_end(DADDR, SADDR, 0x02);
    assert(rx_packet_handler(&ctx, &end_p) == CAPTURE_NOT_ON);
}

void test_set_remote_addr_updates_filter()
{
    rx_handler_ctx_t ctx;
    rx_handler_init(&ctx, noop_command_callback, noop_end_callback, DADDR, SADDR);

    // packet from a different source — rejected initially
    packet p_wrong = make_begin(DADDR, 0x1234, 0x01);
    assert(rx_packet_handler(&ctx, &p_wrong) == PACKET_REJECTED);

    // update expected remote address
    rx_handler_set_remote_addr(&ctx, 0x1234);

    // same packet type with new source — now accepted (use different ack_id to avoid redundancy)
    packet p_right = make_begin(DADDR, 0x1234, 0x02);
    assert(rx_packet_handler(&ctx, &p_right) == CAPTURE_BEGIN);
}

void test_end_fires_callback_and_resets()
{
    reset_callback_state();
    rx_handler_ctx_t ctx;
    rx_handler_init(&ctx, noop_command_callback, noop_end_callback, DADDR, SADDR);

    packet begin = make_begin(DADDR, SADDR, 0x01);
    packet end_p = make_end(DADDR, SADDR, 0x02); // different ack_id avoids redundancy
    rx_packet_handler(&ctx, &begin);
    rx_handler_return ret = rx_packet_handler(&ctx, &end_p);

    assert(ret == CAPTURE_END);
    assert(end_callback_fired == 1);
    // capture should be off now — sending another END returns CAPTURE_NOT_ON
    packet end2 = make_end(DADDR, SADDR, 0x03);
    assert(rx_packet_handler(&ctx, &end2) == CAPTURE_NOT_ON);
}

void test_two_contexts_independent()
{
    rx_handler_ctx_t ctx1, ctx2;
    rx_handler_init(&ctx1, noop_command_callback, noop_end_callback, DADDR,   SADDR);
    rx_handler_init(&ctx2, noop_command_callback, noop_end_callback, 0x1234, 0x5678);

    // packet for ctx1 is accepted by ctx1, rejected by ctx2
    packet p1 = make_begin(DADDR, SADDR, 0x01);
    assert(rx_packet_handler(&ctx1, &p1) == CAPTURE_BEGIN);
    assert(rx_packet_handler(&ctx2, &p1) == PACKET_REJECTED);

    // ctx1 is in capture state, ctx2 is not — they are independent
    packet end_for_ctx2 = make_end(0x1234, 0x5678, 0x01);
    assert(rx_packet_handler(&ctx2, &end_for_ctx2) == CAPTURE_NOT_ON);

    // ctx1 rejects the packet intended for ctx2
    assert(rx_packet_handler(&ctx1, &end_for_ctx2) == PACKET_REJECTED);
}

// =====================================================================
// Full capture flow tests (existing)
// =====================================================================

void test_full_capture_no_redundancy()
{
    reset_callback_state();
    rx_handler_ctx_t ctx;
    char *data_str = TEST_STRING;
    size_t data_len = strlen(data_str) + 1;
    uint8_t *data = malloc(data_len);
    memcpy(data, data_str, data_len);

    rx_handler_init(&ctx, noop_command_callback, capture_end_callback, DADDR, SADDR);

    size_t npackets = (size_t)ceil((double)data_len / payload_length_max) + 2;
    packet **p_arr = malloc(sizeof(packet *) * npackets);

    assert(data_to_packet_array(p_arr, data, data_len, DADDR, SADDR, 0x01, true) == 0);

    for (size_t i = 0; i < npackets; i++)
        rx_packet_handler(&ctx, p_arr[i]);

    assert(end_callback_fired == 1);
    assert(strcmp(reassembled_data, TEST_STRING) == 0);

    free(data);
    free(p_arr);
}

void test_full_capture_with_redundancy()
{
    reset_callback_state();
    rx_handler_ctx_t ctx;
    char *data_str = TEST_STRING;
    size_t data_len = strlen(data_str) + 1;
    uint8_t *data = malloc(data_len);
    memcpy(data, data_str, data_len);

    rx_handler_init(&ctx, noop_command_callback, capture_end_callback, DADDR, SADDR);

    size_t npackets = (size_t)ceil((double)data_len / payload_length_max) + 2;
    packet **p_arr = malloc(sizeof(packet *) * npackets);

    assert(data_to_packet_array(p_arr, data, data_len, DADDR, SADDR, 0x01, true) == 0);

    for (size_t i = 0; i < npackets; i++)
    {
        rx_packet_handler(&ctx, p_arr[i]);
        assert(rx_packet_handler(&ctx, p_arr[i]) == PACKET_REDUNDANT);
    }

    assert(end_callback_fired == 1);

    free(data);
    free(p_arr);
}

void test_command_packet_bypass()
{
    reset_callback_state();
    rx_handler_ctx_t ctx;
    char *data_str = COMMAND_PAYLOAD_STR;
    size_t data_len = strlen(data_str) + 1;
    uint8_t *data = malloc(data_len);
    memcpy(data, data_str, data_len);

    rx_handler_init(&ctx, command_callback, noop_end_callback, DADDR, SADDR);

    size_t npackets = (size_t)ceil((double)data_len / payload_length_max);
    packet **p_arr = malloc(sizeof(packet *) * npackets);

    assert(data_to_packet_array(p_arr, data, data_len, DADDR, SADDR, 0x01, false) == 0);

    for (size_t i = 0; i < npackets; i++)
        rx_packet_handler(&ctx, p_arr[i]);

    assert(command_callback_fired == (int)npackets);

    free(data);
    free(p_arr);
}

void test_burst_with_inline_command_packet()
{
    reset_callback_state();
    rx_handler_ctx_t ctx;
    char *data_str = TEST_STRING;
    size_t data_len = strlen(data_str) + 1;
    uint8_t *data = malloc(data_len);
    memcpy(data, data_str, data_len);

    rx_handler_init(&ctx, command_callback, capture_end_callback, DADDR, SADDR);

    size_t npackets = (size_t)ceil((double)data_len / payload_length_max) + 2;
    packet **p_arr = malloc(sizeof(packet *) * npackets);

    assert(data_to_packet_array(p_arr, data, data_len, DADDR, SADDR, 0x05, true) == 0);

    uint8_t *cmd_payload = malloc(sizeof(COMMAND_PAYLOAD_STR));
    memcpy(cmd_payload, COMMAND_PAYLOAD_STR, sizeof(COMMAND_PAYLOAD_STR));
    packet *cmd_p = packet_constructor(DADDR, SADDR, 0x06, 0,
                                       sizeof(COMMAND_PAYLOAD_STR), cmd_payload);

    for (size_t i = 0; i < npackets; i++)
    {
        rx_packet_handler(&ctx, p_arr[i]);
        assert(rx_packet_handler(&ctx, p_arr[i]) == PACKET_REDUNDANT);
        if (i == 2)
            assert(rx_packet_handler(&ctx, cmd_p) == COMMAND_PACKET);
    }

    assert(end_callback_fired == 1);
    assert(command_callback_fired == 1);

    free(data);
    free(p_arr);
}

int main()
{
    // edge cases
    test_handler_not_configured();
    test_null_packet_returns_null_packet();
    test_wrong_dest_rejected();
    test_wrong_src_rejected();
    test_redundant_packet_rejected();
    test_capture_already_on();
    test_capture_not_on_end_without_begin();
    test_out_of_order_data_rejected();
    test_in_order_data_captured();
    test_handler_reset_clears_capture();
    test_set_remote_addr_updates_filter();
    test_end_fires_callback_and_resets();
    test_two_contexts_independent();

    // full flow
    test_full_capture_no_redundancy();
    test_full_capture_with_redundancy();
    test_command_packet_bypass();
    test_burst_with_inline_command_packet();

    return 0;
}
