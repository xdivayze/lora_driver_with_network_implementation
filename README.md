# LoRa Driver with Network Implementation

A platform-independent C library for the SX1278 LoRa radio module, with a lightweight packet-based networking layer on top.

## What it does

- Initialises and configures the SX1278 radio (frequency, power, modulation parameters)
- Sends and receives raw LoRa payloads
- Fragments large data blobs into a sequenced packet stream and reassembles them on the receiver
- ACKs every received packet; the sender retries until the matching ACK arrives or a timeout is reached
- Spreading factor (SF6–SF12), bandwidth, and coding rate can all be changed at runtime without reinitialising the radio
- Filters duplicate packets and rejects out-of-order data
- Routes single-packet "command" messages to a separate callback without disrupting an in-progress stream capture

## What it does not do

- Provide its own SPI driver — you supply read/write callbacks
- Provide its own timer or RTOS delay — you supply those callbacks too
- Implement MAC-layer security or encryption

## Architecture

```
┌─────────────────────────────────────────────┐
│               Application code              │
└────────────┬──────────────┬─────────────────┘
             │              │
   ┌──────────▼──────┐  ┌───▼──────────────────┐
   │  rx_packet_     │  │  sx127x_rx_utils /   │
   │  handler        │  │  sx127x_driver        │
   │  (state machine)│  │  sx127x_utils         │
   └──────────┬──────┘  └───┬──────────────────┘
              │              │
   ┌──────────▼──────────────▼──────────────────┐
   │            sx127x_config                   │
   │  (holds spi_port, logger, timer contexts)  │
   └──────┬────────────┬───────────────┬────────┘
          │            │               │
   ┌──────▼──┐  ┌──────▼──┐  ┌────────▼──────┐
   │spi_port │  │ logger  │  │ sx127x_timer  │
   │(HAL)    │  │         │  │               │
   └──────┬──┘  └──────┬──┘  └────────┬──────┘
          │            │               │
   ┌──────▼────────────▼───────────────▼──────┐
   │            Platform callbacks            │
   │   (SPI read/write, get_time_us, delay)   │
   └──────────────────────────────────────────┘
```

## Building

Requires CMake >= 3.16 and a C11-capable compiler.

```sh
cmake -B build
cmake --build build
```

To run the test suite:

```sh
cd build && ctest --output-on-failure
```

## Quick start

### 1. Implement the platform callbacks

```c
// SPI
sx127x_err_t my_spi_write(const void *handle, uint8_t reg,
                          const uint8_t *data, int len);
sx127x_err_t my_spi_read (const void *handle, uint8_t reg,
                          uint8_t *data,       int len);

// Timer (returns microseconds since boot)
int64_t my_get_time_us(void);

// Delay
void my_delay_ms(uint64_t ms);

// Logger (optional -- pass NULL to silence)
void my_logger(const char *msg);
```

### 2. Initialise the context structs

```c
spi_port_t         spi;
logger_ctx_t       logger;
sx127x_timer_ctx_t timer;

spi_port_init(&spi, my_spi_write, my_spi_read, &my_spi_handle);
logger_init(&logger, my_logger, LOG_INFO);
sx127x_timer_init(&timer, my_get_time_us, my_delay_ms);

if (initialize_sx_1278(spi, logger, timer) != SX_OK) {
    // version check failed -- check wiring
}
```

### 3. Send and receive

```c
// Send
uint8_t payload[] = { 0x01, 0x02, 0x03 };
sx1278_send_payload(payload, sizeof(payload), /*switch_to_rx=*/1);

// Receive (blocking, waits for RxDone IRQ)
uint8_t buf[255];
size_t  len = 0;
if (sx1278_read_last_payload(buf, &len) == SX_OK) {
    // buf[0..len-1] contains the received bytes
}
```

For streaming large data, use `data_to_packet_array` / `rx_packet_handler` -- see
`include/network_data_operations.h` and `include/rx_packet_handler.h`.

## Radio defaults

The default frequency range, channel bandwidth, and transmit power have been chosen with reference to the ETSI EN 300 220 and LoRa Alliance regional parameters for the EU863-870 band. **These defaults are provided as a starting point only and are not a guarantee of regulatory compliance. It is your responsibility to verify that your deployment meets all applicable local RF regulations before operating the device.**

| Parameter          | Value          |
|--------------------|----------------|
| Frequency range    | 863 - 865 MHz  |
| Channel bandwidth  | 125 kHz        |
| Number of channels | 14             |
| Spreading factor   | SF12           |
| Coding rate        | 4/5            |
| TX power           | 14 dBm         |
| OCP                | 100 mA         |
| Preamble length    | 24 symbols     |
| Symbol timeout     | 255 symbols    |
| Sync word          | 0xD3           |

Spreading factor and bandwidth can be changed at runtime — see `sx_1278_set_spreading_factor()` and `sx1278_set_bandwidth()`.

### Bit rates at BW=125 kHz, CR=4/5

Calculated using the Semtech time-on-air formula (SX1276 datasheet §4.1.1) with CRC on, explicit headers, 24-symbol preamble, and a full-size frame of 15 bytes (7-byte protocol header + 8-byte payload). The effective rate counts only the 8 payload bytes.

| SF   | Frame ToA | Raw PHY rate | Effective payload rate |
|------|-----------|--------------|------------------------|
| SF6  | 34 ms     | ~9375 bps    | ~1887 bps              |
| SF7  | 63 ms     | ~5469 bps    | ~1020 bps              |
| SF8  | 125 ms    | ~3125 bps    | ~510 bps               |
| SF9  | 230 ms    | ~1758 bps    | ~278 bps               |
| SF10 | 461 ms    | ~977 bps     | ~139 bps               |
| SF11 | 840 ms    | ~537 bps     | ~76 bps                |
| SF12 | 1679 ms   | ~293 bps     | ~38 bps                |

## Packet frame format

```
 0       1       2       3       4       5        6..N
+-------+-------+-------+-------+-------+--------+----------+
| dest  | dest  | src   | src   |ack_id |seq_num | payload  |
| addr  | addr  | addr  | addr  |       |        |          |
| (MSB) | (LSB) | (MSB) | (LSB) |       |        |          |
+-------+-------+-------+-------+-------+--------+----------+
```

- **seq_num = 0x00** -- BEGIN (opens a capture session)
- **seq_num = 0xFF** -- END (closes the session, fires the end callback)
- **seq_num = 1..N** -- DATA (sequential; rejected if out of order)
- **no BEGIN/END framing** -- COMMAND (routed directly to the command callback)

## API reference

### `spi_port.h`

| Function | Description |
|---|---|
| `spi_port_init(port, writer, reader, handle)` | Initialise an SPI port context |
| `spi_burst_write_reg(port, reg, data, len)` | Write `len` bytes starting at `reg` |
| `spi_burst_read_reg(port, reg, data, len)` | Read `len` bytes starting at `reg` |

### `logger.h`

| Function | Description |
|---|---|
| `logger_init(ctx, logger, level)` | Initialise a logger context |
| `logger_set_level(ctx, level)` | Change the log level at runtime |
| `network_log(ctx, str, level)` | Log a string at the given level |
| `network_log_with_tag(ctx, tag, str, level)` | Log with a tag prefix |
| `network_log_err(ctx, tag, str)` | Log at `LOG_ERROR` |

### `sx127x_timer.h`

| Function | Description |
|---|---|
| `sx127x_timer_init(ctx, timer, delayer)` | Initialise a timer context |
| `sx127x_timer_get_time_us(ctx)` | Return current time in microseconds |
| `sx127x_task_delay_ms(ctx, ms)` | Block for `ms` milliseconds |

### `sx127x_config.h`

| Function | Description |
|---|---|
| `initialize_sx_1278(spi, logger, timer)` | Full radio initialisation; checks version register |
| `sx_1278_switch_to_nth_channel(n)` | Tune to channel `n` (0-indexed) |
| `calculate_channel_num()` | Return the total number of channels |

### `sx127x_driver.h`

| Function | Description |
|---|---|
| `sx1278_send_payload(data, len, switch_to_rx)` | Transmit `data`; optionally switch to RX continuous afterwards |
| `sx1278_read_last_payload(buf, len)` | Block until RxDone, copy payload into `buf` |
| `poll_for_irq_flag(timeout_us, delay_ms, mask, cleanup)` | Poll IRQ register until `mask` bit set or timeout |

### `sx127x_utils.h`

| Function | Description |
|---|---|
| `send_packet_ensure_ack(p, timeout, ack_type)` | Send a packet and wait for the matching ACK, retrying until timeout |
| `send_burst(p_buf, len)` | Send a full packet array with per-packet ACK confirmation |
| `sx_1278_send_packet(p, switch_to_rx)` | Serialise and transmit a packet struct (no ACK handling) |
| `sx1278_poll_and_read_packet(rx_p, timeout)` | Switch to RX single, wait for a packet, read and parse it |
| `sx_1278_get_channel_rssis(rssi_data, len)` | Read RSSI across all channels using FSK mode — **call `initialize_sx_1278()` afterwards**, as switching to FSK can reset LoRa-mode registers |
| `poll_for_irq_flag_no_timeout(delay_ms, mask, cleanup)` | Poll IRQ register indefinitely until `mask` bit set |

### `rx_packet_handler.h`

| Function | Description |
|---|---|
| `rx_handler_init(ctx, cmd_cb, end_cb, host, remote)` | Initialise a receive handler context |
| `rx_handler_reset(ctx)` | Clear capture state |
| `rx_handler_set_remote_addr(ctx, addr)` | Update the expected sender address |
| `rx_handler_get_captured_array(ctx)` | Return the captured packet array |
| `rx_packet_handler(ctx, packet)` | Process one received packet |

### `network_data_operations.h`

| Function | Description |
|---|---|
| `data_to_packet_array(arr, data, len, dest, src, ack_id, framed)` | Fragment data into a packet array |
| `packet_array_to_data(arr, buf, n)` | Reassemble data from a packet array |

## Error codes

| Code | Meaning |
|---|---|
| `SX_OK` | Success |
| `SX_TIMEOUT` | IRQ did not fire within the timeout |
| `SX_INVALID_STATE` | Radio not in expected state (e.g. no RxDone flag) |
| `SX_INVALID_CRC` | Received packet failed CRC check |
| `SX_INVALID_RESPONSE` | Unexpected value from a register (e.g. version mismatch) |
| `SX_INVALID_ARGUMENT` | Argument out of range |
| `SX_NOT_INITIALIZED` | Context not yet initialised |

## Hardware examples

Tested on **ESP32-C3** (sender) and **ESP32-C6** (receiver) with **ESP-IDF v5.5.2**.

Examples are in `examples/esp32c3_6/` and use the library as an ESP-IDF component via
`examples/esp32c3_6/components/lora_networking/`.

| Example | Location | Description |
|---|---|---|
| `pingpong` | `examples/esp32c3_6/pingpong/` | C3 sends "ping", C6 replies "pong", repeated 5 times |
| `burst` | `examples/esp32c3_6/burst/` | C3 sends a multi-packet burst using `send_burst`; C6 receives and reassembles using `rx_packet_handler` |

Each example has its own `README.md` with build, flash, and monitor instructions.

## Tests

Nine test executables are built under `tests/`:

| Executable | What it covers |
|---|---|
| `test_packets` | Packet construction and field encoding |
| `test_network_data_operations` | Fragmentation and reassembly |
| `test_rx_packet_handler` | RX state machine, deduplication, callbacks |
| `test_logger` | Level filtering, tag format, two-context isolation |
| `test_spi_port` | Write/read dispatch, error propagation, two-port isolation |
| `test_sx127x_timer` | get_time, delay forwarding, two-context isolation |
| `test_sx127x_config` | Channel frequency maths, FRF encoding, init sequence |
| `test_sx127x_driver` | IRQ polling, send/receive state machine |
| `test_sx127x_utils` | `poll_for_irq_flag_no_timeout` |

All tests use mock SPI/timer callbacks -- no hardware required.
