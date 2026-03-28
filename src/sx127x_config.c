#include "sx127x_config.h"
#include "packet.h"
#include <math.h>

#define FXOSC 32000000UL

#define lora_frequency_lf 863000000
#define lora_frequency_hf 865000000
static uint32_t lora_bandwidth_hz = 125000;
static uint8_t spreading_factor = 12;

#define TAG "SX1278 CONFIG DRIVER"

spi_port_t sx127x_spi_port;
logger_ctx_t sx127x_logger;
sx127x_timer_ctx_t sx127x_timer_ctx;

static uint32_t get_bw_value(bandwidths bw)
{
    switch (bw)
    {
    case b62k5:
        return 62500;
    case b125k:
        return 125000;
    case b250k:
        return 250000;
    case b500k:
        return 500000;
    }
    return 0;
}

sx127x_err_t sx1278_read_irq(uint8_t *data)
{
    return spi_burst_read_reg(&sx127x_spi_port, 0x12, data, 1);
}

sx127x_err_t sx1278_clear_irq()
{
    uint8_t data = 0xFF;
    return spi_burst_write_reg(&sx127x_spi_port, 0x12, &data, 1);
}

size_t calculate_channel_num()
{
    return (size_t)floor((lora_frequency_hf - lora_frequency_lf - 2 * lora_bandwidth_hz) / lora_bandwidth_hz);
}

sx127x_err_t sx1278_switch_mode(uint8_t mode_register)
{
    return spi_burst_write_reg(&sx127x_spi_port, 0x01, &mode_register, 1);
}

sx127x_err_t sx_1278_get_op_mode(uint8_t *data)
{
    return spi_burst_read_reg(&sx127x_spi_port, 0x01, data, 1);
}

sx127x_err_t sx_1278_switch_to_nth_channel(size_t n)
{
    sx127x_err_t ret;

    if (n >= calculate_channel_num())
        return SX_INVALID_ARGUMENT;
    uint64_t raw_freq = (uint64_t)(lora_frequency_lf + n * lora_bandwidth_hz + lora_bandwidth_hz / 2);
    uint32_t frf = ((raw_freq) << 19) / FXOSC;
    uint8_t data = (frf >> 16) & 0xFF;

    ret = spi_burst_write_reg(&sx127x_spi_port, 0x06, &data, 1);
    if (ret != SX_OK)
        goto cleanup;

    data = (frf >> 8) & 0xFF;
    ret = spi_burst_write_reg(&sx127x_spi_port, 0x07, &data, 1);
    if (ret != SX_OK)
        goto cleanup;

    data = (frf) & 0xFF;
    ret = spi_burst_write_reg(&sx127x_spi_port, 0x08, &data, 1);

cleanup:
    return ret;
}

sx127x_err_t sx_1278_set_spreading_factor(uint8_t sf)
{
    uint8_t data;
    sx127x_err_t ret;

    ret = sx1278_switch_mode(MODE_LORA | MODE_STDBY);
    if (ret != SX_OK)
    {
        network_log_err(&sx127x_logger, TAG, "couldnt switch to standby");
        return ret;
    }

    if (sf < 6 || sf > 12)
        return SX_INVALID_ARGUMENT;

    data = 0b00000100;
    data |= sf << 4;
    ret = spi_burst_write_reg(&sx127x_spi_port, 0x1E, &data, 1);
    if (ret == SX_OK)
        spreading_factor = sf;

    ret = sx1278_switch_mode(MODE_LORA | MODE_SLEEP);
    if (ret != SX_OK)
        network_log_err(&sx127x_logger, TAG, "error occured while switching to sleep mode");
    return ret;
}

sx127x_err_t sx1278_set_bandwidth(bandwidths bw, coding_rate cr, bool enable_explicit_headers)
{
    uint8_t data;
    sx127x_err_t ret;

    ret = sx1278_switch_mode(MODE_LORA | MODE_STDBY);
    if (ret != SX_OK)
    {
        network_log_err(&sx127x_logger, TAG, "couldnt switch to standby");
        return ret;
    }

    data = ((bw << 4) | (cr << 1));
    if (!enable_explicit_headers)
        data |= 0x01;
    else
        data &= ~0x01;
    ret = spi_burst_write_reg(&sx127x_spi_port, 0x1D, &data, 1);
    if (ret == SX_OK)
    {
        lora_bandwidth_hz = get_bw_value(bw);
        ret = sx_1278_switch_to_nth_channel(0);
        if (ret != SX_OK)
        {
            network_log_err(&sx127x_logger, TAG, "error occured while switching back to 0th channel");
            return ret;
        }
    }

    ret = sx1278_switch_mode(MODE_LORA | MODE_SLEEP);
    if (ret != SX_OK)
        network_log_err(&sx127x_logger, TAG, "error occured while switching to sleep mode");

    return ret;
}

sx127x_err_t initialize_sx_1278(spi_port_t spi, logger_ctx_t logger, sx127x_timer_ctx_t timer)
{
    sx127x_spi_port = spi;
    sx127x_logger = logger;
    sx127x_timer_ctx = timer;

    uint8_t data = 0;
    sx127x_err_t ret;
    ret = spi_burst_read_reg(&sx127x_spi_port, 0x42, &data, 1);
    if (ret != SX_OK)
        return ret;
    if (data != 0x12)
    {
        network_log_err(&sx127x_logger, TAG, "sx1278 register version is not valid");
        return SX_INVALID_RESPONSE;
    }

    ret = sx1278_switch_mode(MODE_LORA | MODE_SLEEP);
    if (ret != SX_OK)
        return ret;

    data = 0x00;
    ret = spi_burst_write_reg(&sx127x_spi_port, 0x0D, &data, 1);
    if (ret != SX_OK)
        return ret;

    ret = spi_burst_write_reg(&sx127x_spi_port, 0x0F, &data, 1);
    if (ret != SX_OK)
        return ret;

    ret = spi_burst_write_reg(&sx127x_spi_port, 0x0E, &data, 1);
    if (ret != SX_OK)
        return ret;

    ret = sx1278_switch_mode(MODE_LORA | MODE_STDBY);
    if (ret != SX_OK)
        return ret;

    ret = sx_1278_switch_to_nth_channel(0);
    if (ret != SX_OK)
        return ret;

    data = 0b11111100;
    ret = spi_burst_write_reg(&sx127x_spi_port, 0x09, &data, 1);
    if (ret != SX_OK)
        return ret;

    data = 0b00101011;
    ret = spi_burst_write_reg(&sx127x_spi_port, 0x2B, &data, 1);
    if (ret != SX_OK)
        return ret;

    data = 0x84;
    ret = spi_burst_write_reg(&sx127x_spi_port, 0x4D, &data, 1);
    if (ret != SX_OK)
        return ret;

    data = 0b01110010;
    ret = spi_burst_write_reg(&sx127x_spi_port, 0x1D, &data, 1);
    if (ret != SX_OK)
        return ret;

    ret = sx_1278_set_spreading_factor(spreading_factor);
    if (ret != SX_OK)
        return ret;

    data = 0xFF;
    ret = spi_burst_write_reg(&sx127x_spi_port, 0x1F, &data, 1);
    if (ret != SX_OK)
        return ret;

    data = 0x00;
    ret = spi_burst_write_reg(&sx127x_spi_port, 0x20, &data, 1);
    if (ret != SX_OK)
        return ret;

    data = 0x18;
    ret = spi_burst_write_reg(&sx127x_spi_port, 0x21, &data, 1);
    if (ret != SX_OK)
        return ret;

    data = 0b11010011; // SFD
    ret = spi_burst_write_reg(&sx127x_spi_port, 0x39, &data, 1);
    if (ret != SX_OK)
        return ret;

    return ret;
}
