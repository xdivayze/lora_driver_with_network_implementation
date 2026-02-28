#pragma once

#include "spi_port.h"
#include <stdlib.h>
#include "sx127x_err.h"
#include <stdbool.h>

#define PHY_TIMEOUT_MSEC 3000


sx127x_err_t sx1278_send_payload(uint8_t *buf, uint8_t len, int switch_to_rx_after_tx);


sx127x_err_t sx1278_read_last_payload(uint8_t *buf, size_t *len);

sx127x_err_t poll_for_irq_flag(size_t timeout_ms, size_t poll_interval_ms, uint8_t irq_and_mask, bool cleanup);
