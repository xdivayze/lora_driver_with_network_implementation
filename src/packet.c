#include "packet.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
static const size_t addr_length = sizeof(uint16_t);

const size_t overhead = 2 * addr_length + 1 + 1 + 1;

const size_t payload_length_max = 8;

const size_t max_frame_size = payload_length_max + overhead;

static void u32_conv_be(uint8_t *dst, uint32_t x)
{
    dst[0] = (uint8_t)(x >> 24);
    dst[1] = (uint8_t)(x >> 16);
    dst[2] = (uint8_t)(x >> 8);
    dst[3] = (uint8_t)(x >> 0);
}

static void u16_conv_be(uint8_t *dst, uint16_t x)
{
    dst[0] = (uint8_t)(x >> 8);
    dst[1] = (uint8_t)(x >> 0);
}

static inline uint32_t read_u32_be(const uint8_t *src)
{
    return ((uint32_t)src[0] << 24) |
           ((uint32_t)src[1] << 16) |
           ((uint32_t)src[2] << 8) |
           ((uint32_t)src[3] << 0);
}

static inline uint16_t read_u16_be(const uint8_t *src)
{
    return ((uint16_t)src[0] << 8) |
           ((uint16_t)src[1] << 0);
}

packet_types check_packet_type(packet *p)
{
    if ((p->sequence_number == 0) && (p->payload_length == 0))
        return PACKET_BEGIN;
    if ((p->sequence_number == UINT8_MAX) && (p->payload_length == 0))
        return PACKET_END;
    if (p->payload_length == 0)
        return PACKET_ACK;
    return PACKET_DATA;
}
bool check_packet_features(packet *p, uint16_t src_addr, uint16_t dest_addr, uint8_t ack_id, uint8_t sequence_number, packet_types packet_type)
{
    if (p->src_address != src_addr)
        return false;
    if (p->dest_address != dest_addr)
        return false;
    if (p->ack_id != ack_id)
        return false;
    if (p->sequence_number != sequence_number)
        return false;
    if (check_packet_type(p) != packet_type)
        return false;
    return true;
}
void packet_description(packet *p, char *buf)
{
    char *str = malloc(9);
    memcpy(str, p->payload, p->payload_length);
    str[p->payload_length] = '\0';
    sprintf(buf, "PACKET TYPE: %i\n DEST_ADDR: 0x%04" PRIx16 "\nSRC_ADDR: 0x%04" PRIx16 "\nACK_ID: 0x%02x\nSEQ: 0x%02x\nPAYLOAD_LENGTH: 0x%02x\nDATA: %s\n", check_packet_type(p), p->dest_address, p->src_address, p->ack_id, p->sequence_number, p->payload_length, str);
    free(str);
}

packet *copy_packet(const packet* p) {
    packet* np = malloc(sizeof(packet));
    uint8_t* np_payload_buf = malloc(p->payload_length);
    memcpy (np,p,sizeof(packet));
    memcpy(np_payload_buf, p->payload, p->payload_length);
    np->payload = np_payload_buf;
    return np;
}
// does not allocate payload
packet *packet_constructor(uint16_t dest_address, uint16_t src_address, uint8_t ack_id,
                           uint8_t sequence_number, uint8_t payload_length, uint8_t *payload)
{
    if (payload_length > payload_length_max)
        return NULL;

    packet *p = malloc(sizeof(packet));
    p->ack_id = ack_id;
    p->dest_address = dest_address;
    p->payload = payload;
    p->payload_length = payload_length;
    p->sequence_number = sequence_number;
    p->src_address = src_address;
    return p;
}

// returns -1 on fail; packet size on success
int parse_packet(uint8_t *packet_data_raw, packet *p)
{
    size_t idx = 0;

    uint16_t dest_addr = read_u16_be(&packet_data_raw[idx]);
    idx += addr_length;

    uint16_t src_addr = read_u16_be(&packet_data_raw[idx]);
    idx += addr_length;

    uint8_t ack_id = packet_data_raw[idx];
    idx += sizeof(ack_id);

    uint8_t sequence_number = packet_data_raw[idx];
    idx += sizeof(sequence_number);

    uint8_t payload_length = packet_data_raw[idx];
    idx++;

    if (payload_length > 0)

    {
        uint8_t *payload = malloc(payload_length);
        memcpy(payload, &(packet_data_raw[idx]), payload_length);
        idx += payload_length;
        p->payload = payload;
    } else {
        p->payload = NULL;
    }

    p->ack_id = ack_id;
    p->dest_address = dest_addr;

    p->payload_length = payload_length;
    p->sequence_number = sequence_number;
    p->src_address = src_addr;

    return idx;
}

// free packet and its payload
void free_packet(packet *p)
{
    if (p->payload)
        free(p->payload);
    free(p);
}

packet *ack_packet(uint16_t dest_address, uint16_t src_address, uint8_t ack_id,
                   uint8_t sequence_number)
{
    return packet_constructor(dest_address, src_address, ack_id, sequence_number, 0, NULL);
}

// returns -1 on fail; packet size on success
// writes pkt to buffer if buffer_size > pkt's frame size
int packet_to_bytestream(uint8_t *buffer, size_t buffer_size, packet *pkt)
{

    if (buffer_size < (overhead + pkt->payload_length))
        return -1;

    size_t idx = 0;
    u16_conv_be(&(buffer[idx]), pkt->dest_address);
    idx += sizeof(pkt->dest_address);
    u16_conv_be(&(buffer[idx]), pkt->src_address);
    idx += sizeof(pkt->src_address);
    buffer[idx] = pkt->ack_id;
    idx += sizeof(pkt->ack_id);
    buffer[idx] = pkt->sequence_number;
    idx += sizeof(pkt->sequence_number);
    buffer[idx] = pkt->payload_length;
    idx += sizeof(pkt->payload_length);

    memcpy(&(buffer[idx]), pkt->payload, pkt->payload_length);
    idx += pkt->payload_length;

    return idx;
}