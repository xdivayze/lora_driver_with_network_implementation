#pragma once
#include "spi_port.h"
#include <stdlib.h>
#include "sx127x_err.h"
#include <stdbool.h>

extern void* sx127x_spi;

#define MODE_LORA (1 << 7)
#define MODE_LF (1 << 3)
#define MODE_SLEEP (0b000 & 0xFF)
#define MODE_STDBY (0b001 & 0xFF)
#define MODE_TX (0b011 & 0xFF)
#define MODE_RX_SINGLE (0b110 & 0xFF)
#define MODE_RX_CONTINUOUS (0b101 & 0xFF)
#define MODE_FSK_RECEIVER (0b101 & 0xFF)

typedef enum
{
    b62k5 = (0b11 << 1),
    b125k = (0b111),
    b250k = (0b1 << 3),
    b500k = (0b1001),
} bandwidths;

typedef enum
{
    cr4d5 = (0b1),
    cr4d6 = (0b10),
    cr4d7 = (0b11),
    cr4d8 = (0b1 << 2),
} coding_rate;

size_t calculate_channel_num();

sx127x_err_t sx1278_read_irq(uint8_t *data);

sx127x_err_t sx1278_clear_irq();

sx127x_err_t sx1278_switch_mode(uint8_t mode_register);

sx127x_err_t sx_1278_get_op_mode(uint8_t *data);

sx127x_err_t sx_1278_switch_to_nth_channel(size_t n);

sx127x_err_t sx_1278_set_spreading_factor(uint8_t sf);

sx127x_err_t sx1278_set_bandwidth(bandwidths bw, coding_rate cr, bool enable_explicit_headers);

sx127x_err_t initialize_sx_1278();