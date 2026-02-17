#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "packet.h"
#include <stdio.h>

int test_parse_packet()
{
    fprintf(stdout, "testing packet parsing\n");
    uint16_t dest_addr = (uint16_t)(rand() & 0xFFFF);
    uint16_t source_addr = (uint16_t)(rand() & 0xFFFF);
    uint8_t ack_id = (uint8_t)(rand() & 0xFF);
    uint8_t sequence_number = 0;
    uint8_t payload_length = 8;

    uint8_t *payload = malloc(payload_length);
    memset(payload, 0xFF, payload_length);

    uint8_t *buf = malloc(max_frame_size);

    packet *p = packet_constructor(dest_addr, source_addr, ack_id, sequence_number, payload_length, payload);

    int result = packet_to_bytestream(buf, max_frame_size, p);

    packet *callback_p = malloc(sizeof(packet));
    int parse_result = parse_packet(buf, callback_p);
    if (parse_result == -1)
    {
        fprintf(stderr, "parser returned -1\n");
        free_packet(p);
        free_packet(callback_p);
        free(buf);
        return -1;
    }

    if ((callback_p->ack_id != ack_id) || (callback_p->dest_address != dest_addr) || (callback_p->payload_length != payload_length) || (callback_p->sequence_number != sequence_number) || (callback_p->src_address != source_addr))
    {
        fprintf(stderr, "packet inconsistency\n");
        free_packet(p);
        free_packet(callback_p);
        free(buf);
        return -1;
    }

    free_packet(p);
    free_packet(callback_p);
    free(buf);
    return 0;
}

int test_packet_to_bytestream()
{
    fprintf(stdout, "testing packet to bytestream conversion\n");
    uint16_t dest_addr = (uint16_t)(rand() & 0xFFFF);
    uint16_t source_addr = (uint16_t)(rand() & 0xFFFF);
    uint8_t ack_id = (uint8_t)(rand() & 0xFF);
    uint8_t sequence_number = 0;
    uint8_t payload_length = 8;

    uint8_t *payload = malloc(payload_length);
    memset(payload, 0xFF, payload_length);

    uint8_t *buf = malloc(max_frame_size);

    packet *p = packet_constructor(dest_addr, source_addr, ack_id, sequence_number, payload_length, payload);

    int result = packet_to_bytestream(buf, max_frame_size, p);

    if (result == -1)
    {
        fprintf(stderr, "packet to bytestream returned -1\n");
        free_packet(p);
        free(buf);
        return -1;
    }

    if (result != max_frame_size)
    {
        fprintf(stderr, "packet to bytestream returned non equal frame size\n");
        free_packet(p);
        free(buf);
        return -1;
    }

    free_packet(p);
    free(buf);
    return 0;
}



int main()
{
    assert(test_packet_to_bytestream() == 0);
    assert(test_parse_packet() == 0);
    return 0;
}