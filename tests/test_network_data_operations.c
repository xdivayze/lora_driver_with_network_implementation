#include "network_data_operations.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <inttypes.h>

int test_packet_array_to_data()
{
    char *data_str = "amumumumu sejuani";
    size_t data_len = strlen(data_str) + 1;
    uint8_t *data = malloc(data_len);
    uint16_t daddr = 0xFF00;

    uint8_t data_str2[256];

    memcpy(data, data_str, data_len);

    size_t npackets = (size_t)ceil((double)data_len / payload_length_max);
    packet **p_arr = malloc(sizeof(packet *) * npackets);
    int ret;
    if (data_to_packet_array(p_arr, data, data_len, daddr, 0x00FF, 0x01, false))
    {
        ret = -1;
        goto cleanup;
    }

    if (packet_array_to_data(p_arr, data_str2, npackets))
    {
        ret = -1;
        goto cleanup;
    }
    if (strcmp(data_str2, data))
    {
        ret = -1;
        goto cleanup;
    }
    ret = 0;
cleanup:
    free(p_arr);
    free(data);
    return ret;
}

int test_data_to_packet_array()
{
    char *data_str = "amumumumu sejuani";
    size_t data_len = strlen(data_str);
    uint8_t *data = malloc(data_len);
    uint16_t daddr = 0xFF00;

    memcpy(data, data_str, data_len);

    size_t npackets = (size_t)ceil((double)data_len / payload_length_max) + 2;
    packet **p_arr = malloc(sizeof(packet *) * npackets);
    int ret;
    if (data_to_packet_array(p_arr, data, data_len, daddr, 0x00FF, 0x01, true))
    {
        ret = -1;
    }

    if (p_arr[npackets - 1] == NULL)
    {
        ret = -1;
        goto cleanup;
    }

    if (p_arr[npackets - 1]->dest_address != daddr)
    {
        ret = -1;
        goto cleanup;
    }

cleanup:

    for (int i = 0; i < npackets; i++)
    {
        if (p_arr[i])
            free_packet(p_arr[i]);
    }

    free(p_arr);

    return ret;
}

int main()
{

    assert(test_data_to_packet_array() == 0);
    assert(test_packet_array_to_data() == 0);
    return 0;
}