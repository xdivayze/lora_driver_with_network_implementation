#pragma once
#include "sx127x_err.h"
#include <stdint.h>
#include "packet.h"
#include "rx_packet_handler.h"

sx127x_err_t start_rx_loop(rx_handler_ctx_t *handler);

sx127x_err_t read_last_packet(packet *p);
