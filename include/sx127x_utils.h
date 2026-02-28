#pragma once

#include "packet.h"
#include "sx127x_err.h"
extern size_t ack_timeout_msec;

// send packet. does not concern itself with acks
sx127x_err_t sx_1278_send_packet(packet *p, int switch_to_rx_after_tx);

// send len packets from p_buf and expect acks.
sx127x_err_t send_burst(packet **p_buf, const int len);

sx127x_err_t send_packet_ensure_ack(packet *p, int timeout, packet_types ack_type);

sx127x_err_t sx_1278_get_channel_rssis(double *rssi_data, size_t *len);
sx127x_err_t sx1278_poll_and_read_packet(packet *rx_p, int timeout);

sx127x_err_t poll_for_irq_flag_no_timeout(size_t poll_interval_ms, uint8_t irq_and_mask, bool cleanup);
