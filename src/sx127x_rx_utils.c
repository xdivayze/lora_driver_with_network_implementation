#include "sx127x_rx_utils.h"
#include "packet.h"
#include "sx127x_driver.h"
#include "logger.h"
#include <stdlib.h>
#include "sx127x_utils.h"
#include "sx127x_config.h"
#include "rx_packet_handler.h"
#include <string.h>
#define TAG "RX_UTILS"
sx127x_err_t start_rx_loop()
{
    sx127x_err_t ret;
    uint8_t data;

    packet *rx_p = malloc(sizeof(packet));
    rx_p->payload = NULL;

    char *p_desc = malloc(2048);

    ret = sx1278_switch_mode(MODE_LORA | MODE_RX_CONTINUOUS);
    if (ret != SX_OK)
    {
        network_log_err(TAG, "error occured while switching mode");
        goto cleanup;
    }

    while (1)
    {
        ret = sx1278_clear_irq();
        if (ret != SX_OK)
        {
            network_log_err(TAG, "couldnt clear irq");
            goto cleanup;
        }

        ret = sx1278_switch_mode(MODE_LORA | MODE_RX_CONTINUOUS);
        if (ret != SX_OK)
        {
            network_log_err(TAG, "error occured while switching mode");
            goto cleanup;
        }

        ret = poll_for_irq_flag_no_timeout(1, (1 << 6), false);
        if (ret != SX_OK)
        {
            network_log_err(TAG, "failed poll for packet received flag. ");
            continue;
        }

        ret = read_last_packet(rx_p);
        if (ret != SX_OK)
        {
            network_log_err(TAG, "error occured while reading the last packet");
            continue;
        }

        ret = sx_1278_send_packet(ack_packet(rx_p->src_address, rx_p->dest_address, rx_p->ack_id, rx_p->sequence_number), false);
        if (ret != SX_OK)
        {
            network_log_err(TAG, "error occured while sending the ack packet");
            goto cleanup;
        }

        // packet_description(rx_p, p_desc);
        // ESP_LOGI(TAG, "received  packet:\n%s", p_desc);

        rx_packet_handler(rx_p);
    }

cleanup:
    free_packet(rx_p);
    return ret;
}

// uses irq flags to check if rxdone is set but does not poll for the flag. for polling use poll_for_irq_flag
// resets irq
// assumes standby mode
// allocates packet payload
sx127x_err_t read_last_packet(packet *p)
{
    static uint8_t rx_buffer[255];
    size_t len = 0;
    sx127x_err_t ret = sx1278_read_last_payload(rx_buffer, &len);
    int packet_size = parse_packet(rx_buffer, p);
    if (packet_size == -1)
    {
        network_log_err(TAG, "packet couldnt be parsed, discarding packet\n");

        ret = SX_INVALID_STATE;
        goto cleanup;
    }

    ret = SX_OK;

cleanup:
    memset(rx_buffer, 0x00, sizeof(rx_buffer)); // fill rx buffer with zeros
    sx1278_clear_irq();
    return ret;
}