#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

extern const size_t payload_length_max;
extern const size_t overhead;
extern const size_t max_frame_size;

#define SFD 0b11010011;

typedef enum
{
    PACKET_ACK = 0,   // payload length is 0 and sequence number is non zero
    PACKET_BEGIN = 1, // payload length and sequence number is 0
    PACKET_END = 2,   // sequence number is uint32_max and payload length is 0
    PACKET_DATA = 3,
} packet_types;

typedef struct
{
    uint16_t dest_address;
    uint16_t src_address;
    uint8_t ack_id;
    uint8_t sequence_number;
    uint8_t payload_length;
    uint8_t *payload;
} packet;

void packet_description(packet* p, char* buf);

packet *ack_packet(uint16_t dest_address, uint16_t src_address, uint8_t ack_id,
                   uint8_t sequence_number);

packet *packet_constructor(uint16_t dest_address, uint16_t src_address, uint8_t ack_id,
                           uint8_t sequence_number, uint8_t payload_length, uint8_t *payload);

packet *copy_packet(const packet* p);
void free_packet(packet *p);

int packet_to_bytestream(uint8_t *buffer, size_t buffer_size, packet *pkt);

int parse_packet(uint8_t *packet_data_raw, packet *p);
packet_types check_packet_type(packet *p);
bool check_packet_features(packet *p, uint16_t src_addr, uint16_t dest_addr, uint8_t ack_id, uint8_t sequence_number, packet_types packet_type);
#endif