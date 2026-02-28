#include "sx127x_utils.h"
#include "spi_port.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "sx127x_err.h"
#include "sx127x_timer.h"
#include <stdbool.h>
#include "logger.h"
#include "sx127x_driver.h"
#include "sx127x_config.h"
#include "sx127x_rx_utils.h"

#define TAG "sx_1278_utils"



sx127x_err_t   sx1278_poll_and_read_packet(packet *rx_p, int timeout)
{
    sx127x_err_t ret;
    uint8_t data;
    ret = sx1278_switch_mode(MODE_LORA | MODE_RX_SINGLE);
    if (ret != SX_OK)
    {
        network_log_err(TAG, "couldnt switch to rx single");
        goto cleanup;
    }

    ret = poll_for_irq_flag(timeout, 1, 1 << 6, false);
    if (ret != SX_OK)
    {
        network_log_err(TAG, "couldn't poll for packet received flag, got. ");
        goto cleanup;
    }

    ret = sx1278_switch_mode(MODE_LORA | MODE_STDBY);
    if (ret != SX_OK)
    {
        network_log_err(TAG, "couldnt switch to standby");
        goto cleanup;
    }

    ret = read_last_packet(rx_p);
    if (ret != SX_OK)
    {
        network_log_err(TAG, "error occured while reading the last packet");
        goto cleanup;
    }

    ret = SX_OK;

cleanup:
    sx1278_clear_irq();
    sx1278_switch_mode(MODE_LORA | MODE_STDBY);
    return ret;
}

// this one only sends packets in the array and waits for acks in between always include the BEGIN and END packets in the array
// consumes packets in p_buf
sx127x_err_t send_burst(packet **p_buf, const int len)
{
    packet *curr_packet;
    sx127x_err_t ret;
    uint8_t data = 0;
    packet_types ptype;

    for (int i = 0; i < len; i++)
    {

        curr_packet = p_buf[i];

        if (i == 0)
            ptype = PACKET_BEGIN;
        else if (i == len - 1)
            ptype = PACKET_END;
        else
            ptype = PACKET_ACK;

        ret = send_packet_ensure_ack(curr_packet, 4 * PHY_TIMEOUT_MSEC, ptype);
        if (ret != SX_OK)
            goto cleanup;
        free(p_buf[i]);
    }
    ret = SX_OK;
cleanup:
    return ret;
}

// puts into standby and returns back to standby if switch_to_rx_after_tx is not set.
// internally calls packet_to_bytestream() function
// polls for TxDone and clears irq flags
sx127x_err_t sx_1278_send_packet(packet *p, int switch_to_rx_after_tx)
{
    static uint8_t tx_buffer[255];
    char* p_desc = malloc(255);
    packet_description(p, p_desc);
    network_log_with_tag(TAG,"sending packet", LOG_INFO_LOW);
    free(p_desc);

    int packet_size = packet_to_bytestream(tx_buffer, sizeof(tx_buffer), p);

    if (packet_size == -1)
    {
        network_log_err(TAG, "couldnt convert packet to bytesteam\n");
        return SX_INVALID_STATE;
    }

    uint8_t packet_size_byte = (uint8_t)(packet_size & 0xFF);

    sx127x_err_t ret = sx1278_send_payload(tx_buffer, packet_size, switch_to_rx_after_tx);
    return ret;
}

#define RSSI_READ_DELAY_MS 20

static sx127x_err_t switch_to_fsk()
{
    uint8_t data = 0x00;
    sx127x_err_t ret;
    ret = sx1278_switch_mode((MODE_LORA | MODE_SLEEP));
    if (ret != SX_OK)
    {
        network_log_err(TAG, "couldnt set sleep mode on lora\n");
        return ret;
    }

    ret = sx1278_switch_mode(MODE_SLEEP);
    if (ret != SX_OK)
    {
        network_log_err(TAG, "couldnt set sleep mode on fsk\n");
        return ret;
    }

    ret = sx_1278_get_op_mode(&data);
    if (ret != SX_OK)
    {
        network_log_err(TAG, "couldnt read operation mode register");
        return ret;
    }
    if (data >> 7)
    {
        network_log_err(TAG, "operational mode stuck at lora ");
        return ret;
    }

    return ret;
}

// timeout / PHY_TIMEOUT used
sx127x_err_t send_packet_ensure_ack(packet *p, int timeout, packet_types ack_type)
{
    packet *rx_p = malloc(sizeof(packet));
    rx_p->payload = NULL;
    uint8_t data = 0;
    sx127x_err_t ret;
    char *p_desc = malloc(2048);
    int timeout_n = (int)ceilf((float)timeout / PHY_TIMEOUT_MSEC);
    // try again if ack timeout, wrong ack
    for (int i = 0; i < timeout_n; i++)
    {
        ret = sx_1278_send_packet(p, 1);
        if (ret != SX_OK)
        {
            network_log_err(TAG, "error occured while sending the packet");
            goto cleanup;
        }

        ret = sx1278_poll_and_read_packet(rx_p, PHY_TIMEOUT_MSEC);
        if (ret != SX_OK)
        {
            network_log_with_tag(TAG, "error occured while reading packet. retrying...", LOG_INFO_LOW);
            continue;
        }

        if (!check_packet_features(rx_p, p->dest_address, p->src_address, p->ack_id, p->sequence_number, ack_type))
        {
            packet_description(rx_p, p_desc);

            network_log_with_tag(TAG, "mismatched packet. retrying...", LOG_INFO_LOW);
            continue;
        }

        network_log_with_tag(TAG,"ack accepted", LOG_INFO_LOW);

        ret = SX_OK;
        goto cleanup;
    }

    ret = SX_TIMEOUT;

cleanup:
    sx1278_clear_irq();
    free_packet(rx_p);
    free(p_desc);
    sx1278_switch_mode(MODE_LORA | MODE_SLEEP);
    return ret;
}

// switching to FSK/OOK mode discondifures lora. be sure te recall sx_1278_init() again
// rssi_data gets values in dBm
sx127x_err_t sx_1278_get_channel_rssis(double *rssi_data, size_t *len)
{

    sx127x_err_t ret = switch_to_fsk();
    if (ret != SX_OK)
    {
        network_log_err(TAG, "couldnt switch to fsk mode\n");
        return ret;
    }

    uint8_t data;
    ret = sx1278_switch_mode(MODE_FSK_RECEIVER);
    if (ret != SX_OK)
    {
        network_log_err(TAG, "couldnt set rx mode on fsk\n");
        return ret;
    }

    size_t channel_n = calculate_channel_num();
    double *temp_rssi_arr = malloc(channel_n * sizeof(double));
    uint8_t ret_data;

    for (size_t i = 0; i < channel_n; i++)
    {
        ret_data = 0;
        ret = sx_1278_switch_to_nth_channel(i);
        if (ret != SX_OK)
        {
            if (i == channel_n - 1)
                network_log_err(TAG, "couldnt switch to channel");
            else
                network_log_err(TAG, "couldnt switch to channel, trying next one");

            continue;
        }

        sx127x_task_delay_ms(RSSI_READ_DELAY_MS);


        ret = spi_burst_read_reg(sx127x_spi, 0x11, &ret_data, 1);
        if (ret != SX_OK)
        {
            if (i == channel_n - 1)
                network_log_err(TAG, "couldnt read rssi on channel.");
            else
                network_log_err(TAG, "couldnt read rssi on channel. switching to next channel");
            continue;
        }

        temp_rssi_arr[i] = -ret_data / 2.0;
    }

    *len = channel_n;
    memcpy(rssi_data, temp_rssi_arr, channel_n * sizeof(double));

    free(temp_rssi_arr);
    ret = sx1278_switch_mode(MODE_SLEEP | MODE_LORA);
    if (ret != SX_OK)
    {
        network_log_err(TAG, "couldnt switch back to lora sleep mode");
    }
    return ret;
}

sx127x_err_t poll_for_irq_flag_no_timeout(size_t poll_interval_ms, uint8_t irq_and_mask, bool cleanup)
{

    uint8_t irq = 0;
    sx127x_err_t ret;
    uint8_t data;
    while (1)
    {
        ret = sx1278_read_irq(&irq);
        if (ret != SX_OK)
        {
            network_log_err(TAG, "couldnt read irq register");
            if (cleanup)
            {
                data = 0xFF;
                spi_burst_write_reg(sx127x_spi, 0x12, &data, 1); // clear irq flags
            }
            return ret;
        }
        
        
        if (irq & irq_and_mask)
        {
            
            if (cleanup)
            {
                data = 0xFF;
                spi_burst_write_reg(sx127x_spi, 0x12, &data, 1); // clear irq flags
            }
            return SX_OK;
        }

        sx127x_task_delay_ms(poll_interval_ms);

    }
}