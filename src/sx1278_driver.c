#include "sx1278_driver.h"
#include "spi_port.h"
#include "packet.h"
#include "logger.h"
#include <math.h>
#include "sx127x_timer.h"
#include <stdbool.h>
#include "sx127x_config.h"

#define TAG "sx_1278_driver"

// len is the callback length
sx127x_err_t sx1278_read_last_payload(uint8_t *buf, size_t *len)
{
    uint8_t data = 0;
    sx127x_err_t ret;

    ret = sx1278_switch_mode(MODE_LORA | MODE_STDBY);
    if (ret != SX_OK)
    {
        sx127x_log_err(TAG, "error occured while switching mode");
        return ret;
    }

    ret = sx1278_read_irq(&data);

    if (ret != SX_OK)
        return ret;
    uint8_t rx_done_mask = 0b01000000; // RxDone bit

    if (!(data & rx_done_mask))
    {
        sx1278_clear_irq();

        sx127x_log_err(TAG, "rx read not done\n");
        return SX_INVALID_STATE;
    }
    uint8_t rx_crc_mask = rx_done_mask >> 1; // PayloadCrcError bit
    if (data & rx_crc_mask)
    {
        sx1278_clear_irq();

        sx127x_log_err(TAG, "rx crc failed, discarding packet\n");
        return SX_INVALID_CRC;
    }

    uint8_t n_packet_bytes = 0;
    ret = spi_burst_read_reg(sx127x_spi, 0x13, &n_packet_bytes, 1); // read number of bytes

    uint8_t current_fifo_addr = 0;
    ret = spi_burst_read_reg(sx127x_spi, 0x10, &current_fifo_addr, 1);
    if (ret != SX_OK)
    {
        sx1278_clear_irq();
        sx127x_log_err(TAG, "couldnt get rx fifo pointer, skipping packet\n");
        return ret;
    }

    ret = spi_burst_write_reg(sx127x_spi, 0x0D, &current_fifo_addr, 1);
    if (ret != SX_OK)
    {
        sx1278_clear_irq();

        sx127x_log_err(TAG, "couldnt set rx fifo pointer, skipping packet\n");
        return ret;
    }

    ret = spi_burst_read_reg(sx127x_spi, 0x00, buf, n_packet_bytes); // read payload
    if (ret != SX_OK)
    {
        sx1278_clear_irq();
        sx127x_log_err(TAG, "couldnt read rx fifo, skipping packet\n");
        return ret;
    }

    sx1278_clear_irq();

    *len = n_packet_bytes;
    return SX_OK;
}

sx127x_err_t sx1278_send_payload(uint8_t *buf, uint8_t len, int switch_to_rx_after_tx)
{

    uint8_t data = 0x00;
    sx127x_err_t ret = sx1278_switch_mode((MODE_LORA | MODE_STDBY));
    if (ret != SX_OK)
    {
        sx127x_log_err(TAG, "couldnt put sx1278 in standby mode\n");
        return ret;
    }

    data = 0x00;                                          // TODO refactor fifo read/writes
    ret = spi_burst_read_reg(sx127x_spi, 0x0E, &data, 1); // read fifo tx base pointer
    if (ret != SX_OK)
    {
        sx127x_log_err(TAG, "couldnt set fifo address\n");
        return ret;
    }
    ret = spi_burst_write_reg(sx127x_spi, 0x0D, &data, 1); // set fifo pointer
    if (ret != SX_OK)
    {
        sx127x_log_err(TAG, "couldnt set fifo address\n");
        return ret;
    }

    ret = spi_burst_write_reg(sx127x_spi, 0x22, &len, 1); // write payload length
    if (ret != SX_OK)
    {
        sx127x_log_err(TAG, "couldnt write packet length\n");
        return ret;
    }
    ret = spi_burst_write_reg(sx127x_spi, 0x00, buf, len); // write payload
    if (ret != SX_OK)
    {
        sx127x_log_err(TAG, "couldnt write packet to tx fifo\n");
        return ret;
    }

    ret = sx1278_clear_irq();

    if (ret != SX_OK)
    {

        sx127x_log_err(TAG, "couldnt reset irq flags\n");
        return ret;
    }

    ret = sx1278_switch_mode((MODE_LORA | MODE_TX));
    if (ret != SX_OK)
    {

        sx127x_log_err(TAG, "couldnt switch to transmit mode\n");
        return ret;
    }

    ret = poll_for_irq_flag(3000, 3, (1 << 3), true); // poll until tx done flag is set
    if (ret != SX_OK)
    {
        sx127x_log_err(TAG, "failed while polling for irq tx done flag\n");
        return ret;
    }

    ret = sx1278_clear_irq();

    if (ret != SX_OK)
    {

        sx127x_log_err(TAG, "couldnt reset irq flags\n");
        return ret;
    }

    data = switch_to_rx_after_tx ? MODE_RX_CONTINUOUS : MODE_SLEEP;
    ret = sx1278_switch_mode((MODE_LORA | data));

    if (ret != SX_OK)
    {
        sx127x_log_err(TAG, "couldnt put sx1278 in next mode after tx is complete\n");
        return ret;
    }

    return SX_OK;
}

sx127x_err_t poll_for_irq_flag(size_t timeout_ms, size_t poll_interval_ms, uint8_t irq_and_mask, bool cleanup)
{
    timeout_ms = (timeout_ms <= 0) ? 3000 : timeout_ms;
    poll_interval_ms = (poll_interval_ms <= 0) ? 2 : poll_interval_ms;

    const int64_t start = sx127x_timer_get_time_us();
    const int64_t timeout_us = (int64_t)timeout_ms * 1000;

    uint8_t irq = 0;
    sx127x_err_t ret;

    int64_t elapsed_us = 0;
    uint8_t data;
    while (1)
    {
        ret = spi_burst_read_reg(sx127x_spi, 0x12, &irq, 1);
        if (ret != SX_OK)
        {
            sx127x_log_err(TAG, "couldnt read irq register\n");
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
        elapsed_us = sx127x_timer_get_time_us() - start;
        if (elapsed_us > timeout_us)
        {
            if (cleanup)
            {
                data = 0xFF;
                spi_burst_write_reg(sx127x_spi, 0x12, &data, 1); // clear irq flags
            }
            sx127x_log_err(TAG, "polling timeout");
            return SX_TIMEOUT;
        }

        vTaskDelay(pdMS_TO_TICKS(poll_interval_ms));
    }
}