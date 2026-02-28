#pragma once
#include "sx127x_err.h"
#include <stdint.h>
#include "packet.h"
sx127x_err_t start_rx_loop();
sx127x_err_t read_last_packet(packet *p);