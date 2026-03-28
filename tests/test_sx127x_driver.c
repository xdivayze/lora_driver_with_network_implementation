#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "sx127x_driver.h"
#include "sx127x_config.h"
#include "sx127x_timer.h"
#include "logger.h"
#include "spi_port.h"

// -----------------------------------------------------------------------
// Mock infrastructure
// -----------------------------------------------------------------------

typedef struct { uint8_t reg; uint8_t value; } reg_write_t;

static reg_write_t write_log[256];
static int         write_log_n = 0;
static uint8_t     read_responses[256];

// Counting reader: returns IRQ flag for reg 0x12 after 'irq_fires_on_read' reads.
static int irq_read_count    = 0;
static int irq_fires_on_read = 1;
static uint8_t irq_mask_value = 0x08; // TxDone by default

static void reset_state()
{
    write_log_n       = 0;
    irq_read_count    = 0;
    irq_fires_on_read = 1;
    irq_mask_value    = 0x08;
    memset(write_log,       0, sizeof(write_log));
    memset(read_responses,  0, sizeof(read_responses));
}

static sx127x_err_t recording_writer(const void *spi, const uint8_t reg, const uint8_t *data, int len)
{
    for (int i = 0; i < len && write_log_n < 256; i++)
        write_log[write_log_n++] = (reg_write_t){.reg = reg, .value = data[i]};
    return SX_OK;
}

static sx127x_err_t counting_reader(const void *spi, const uint8_t reg, uint8_t *data, int len)
{
    if (reg == 0x12) {
        irq_read_count++;
        data[0] = (irq_read_count >= irq_fires_on_read) ? irq_mask_value : 0x00;
    } else {
        for (int i = 0; i < len; i++)
            data[i] = read_responses[reg];
    }
    return SX_OK;
}

// Advancing timer: returns 0 on first call, then a value past any 3-second timeout.
static int   time_call_n  = 0;
static int64_t zero_timer()      { return 0; }
static int64_t advancing_timer() { return (time_call_n++ == 0) ? 0 : 4000000LL; }
static void    noop_delay(uint64_t ms) { (void)ms; }

static void setup(sx127x_timer_t timer_fn)
{
    time_call_n = 0;
    spi_port_init(&sx127x_spi_port, recording_writer, counting_reader, NULL);
    sx127x_timer_init(&sx127x_timer_ctx, timer_fn, noop_delay);
    sx127x_logger = (logger_ctx_t){0};
}

static int wrote(uint8_t reg, uint8_t value)
{
    for (int i = 0; i < write_log_n; i++)
        if (write_log[i].reg == reg && write_log[i].value == value)
            return 1;
    return 0;
}

static uint8_t last_write_to(uint8_t reg)
{
    for (int i = write_log_n - 1; i >= 0; i--)
        if (write_log[i].reg == reg)
            return write_log[i].value;
    return 0xFF;
}

// -----------------------------------------------------------------------
// poll_for_irq_flag tests
// -----------------------------------------------------------------------

void test_poll_irq_fires_immediately()
{
    reset_state();
    setup(zero_timer);
    irq_fires_on_read = 1;
    irq_mask_value    = 1 << 3;

    sx127x_err_t ret = poll_for_irq_flag(3000, 1, 1 << 3, false);
    assert(ret == SX_OK);
    assert(irq_read_count == 1);
}

void test_poll_irq_fires_after_three_reads()
{
    reset_state();
    setup(zero_timer);
    irq_fires_on_read = 3;
    irq_mask_value    = 1 << 3;

    sx127x_err_t ret = poll_for_irq_flag(3000, 1, 1 << 3, false);
    assert(ret == SX_OK);
    assert(irq_read_count == 3);
}

void test_poll_irq_timeout()
{
    reset_state();
    setup(advancing_timer);
    irq_fires_on_read = 999; // IRQ never fires within the test
    irq_mask_value    = 1 << 3;

    sx127x_err_t ret = poll_for_irq_flag(3000, 1, 1 << 3, false);
    assert(ret == SX_TIMEOUT);
}

void test_poll_irq_cleanup_writes_ff_on_success()
{
    reset_state();
    setup(zero_timer);
    irq_fires_on_read = 1;
    irq_mask_value    = 1 << 3;

    poll_for_irq_flag(3000, 1, 1 << 3, true); // cleanup = true
    assert(wrote(0x12, 0xFF));
}

void test_poll_irq_cleanup_writes_ff_on_timeout()
{
    reset_state();
    setup(advancing_timer);
    irq_fires_on_read = 999;

    poll_for_irq_flag(3000, 1, 1 << 3, true); // cleanup = true
    assert(wrote(0x12, 0xFF));
}

void test_poll_irq_no_cleanup_does_not_write_on_success()
{
    reset_state();
    setup(zero_timer);
    irq_fires_on_read = 1;
    irq_mask_value    = 1 << 3;

    poll_for_irq_flag(3000, 1, 1 << 3, false); // cleanup = false
    // With cleanup=false, reg 0x12 must never be written
    for (int i = 0; i < write_log_n; i++)
        assert(write_log[i].reg != 0x12);
}

void test_poll_checks_correct_bit_mask()
{
    reset_state();
    setup(advancing_timer);
    // Reader returns RxDone (0x40) immediately, but we are polling for TxDone (0x08).
    // The wrong bit should not satisfy the mask, so the call times out.
    irq_fires_on_read = 1;
    irq_mask_value    = 0x40; // RxDone — does not match TxDone mask (0x08)

    sx127x_err_t ret = poll_for_irq_flag(3000, 1, 1 << 3, false);
    assert(ret == SX_TIMEOUT);
}

// -----------------------------------------------------------------------
// sx1278_read_last_payload tests
// -----------------------------------------------------------------------

void test_read_payload_no_rxdone_returns_error()
{
    reset_state();
    setup(zero_timer);
    // TxDone bit set but NOT RxDone — function should return INVALID_STATE
    irq_fires_on_read = 1;
    irq_mask_value    = 0x08; // TxDone only (bit 3), RxDone (bit 6) not set

    uint8_t buf[32];
    size_t len = 0;
    sx127x_err_t ret = sx1278_read_last_payload(buf, &len);
    assert(ret == SX_INVALID_STATE);
    assert(wrote(0x12, 0xFF)); // IRQ cleared on error
}

void test_read_payload_crc_error_returns_error()
{
    reset_state();
    setup(zero_timer);
    irq_fires_on_read = 1;
    irq_mask_value    = 0x60; // RxDone (0x40) | PayloadCrcError (0x20)

    uint8_t buf[32];
    size_t len = 0;
    sx127x_err_t ret = sx1278_read_last_payload(buf, &len);
    assert(ret == SX_INVALID_CRC);
    assert(wrote(0x12, 0xFF));
}

void test_read_payload_success()
{
    reset_state();
    setup(zero_timer);
    irq_fires_on_read    = 1;
    irq_mask_value       = 0x40; // RxDone only
    read_responses[0x13] = 4;    // 4 bytes received
    read_responses[0x10] = 0x00; // current FIFO addr
    read_responses[0x00] = 0xAB; // payload bytes

    uint8_t buf[32] = {0};
    size_t len = 0;
    sx127x_err_t ret = sx1278_read_last_payload(buf, &len);
    assert(ret == SX_OK);
    assert(len == 4);
    for (int i = 0; i < 4; i++)
        assert(buf[i] == 0xAB);
}

void test_read_payload_sets_fifo_pointer_before_read()
{
    reset_state();
    setup(zero_timer);
    irq_fires_on_read    = 1;
    irq_mask_value       = 0x40; // RxDone
    read_responses[0x13] = 1;
    read_responses[0x10] = 0x20; // FIFO addr = 0x20

    uint8_t buf[32];
    size_t len = 0;
    sx1278_read_last_payload(buf, &len);
    // FIFO pointer (0x0D) should be set to the value read from 0x10
    assert(last_write_to(0x0D) == 0x20);
}

// -----------------------------------------------------------------------
// sx1278_send_payload tests
// -----------------------------------------------------------------------

void test_send_payload_writes_payload_length()
{
    reset_state();
    setup(zero_timer);
    // TxDone must be set so the poll returns SX_OK
    read_responses[0x0E] = 0x00; // TX FIFO base
    irq_fires_on_read    = 1;
    irq_mask_value       = 1 << 3; // TxDone

    uint8_t payload[] = {0x01, 0x02, 0x03};
    sx127x_err_t ret = sx1278_send_payload(payload, sizeof(payload), 0);
    assert(ret == SX_OK);
    // Payload length written to reg 0x22
    assert(wrote(0x22, sizeof(payload)));
}

void test_send_payload_enters_tx_mode()
{
    reset_state();
    setup(zero_timer);
    read_responses[0x0E] = 0x00;
    irq_fires_on_read    = 1;
    irq_mask_value       = 1 << 3;

    uint8_t payload[] = {0xAA};
    sx1278_send_payload(payload, sizeof(payload), 0);
    // TX mode: MODE_LORA (0x80) | MODE_TX (0x03) = 0x83
    assert(wrote(0x01, 0x80 | 0x03));
}

void test_send_payload_switches_to_sleep_after_tx()
{
    reset_state();
    setup(zero_timer);
    read_responses[0x0E] = 0x00;
    irq_fires_on_read    = 1;
    irq_mask_value       = 1 << 3;

    uint8_t payload[] = {0xBB};
    sx1278_send_payload(payload, sizeof(payload), 0); // switch_to_rx = 0 → sleep
    // Sleep: MODE_LORA (0x80) | MODE_SLEEP (0x00) = 0x80
    assert(last_write_to(0x01) == 0x80);
}

void test_send_payload_switches_to_rx_after_tx()
{
    reset_state();
    setup(zero_timer);
    read_responses[0x0E] = 0x00;
    irq_fires_on_read    = 1;
    irq_mask_value       = 1 << 3;

    uint8_t payload[] = {0xCC};
    sx1278_send_payload(payload, sizeof(payload), 1); // switch_to_rx = 1 → RX continuous
    // RX continuous: MODE_LORA (0x80) | MODE_RX_CONTINUOUS (0x05) = 0x85
    assert(last_write_to(0x01) == 0x85);
}

int main()
{
    // poll_for_irq_flag
    test_poll_irq_fires_immediately();
    test_poll_irq_fires_after_three_reads();
    test_poll_irq_timeout();
    test_poll_irq_cleanup_writes_ff_on_success();
    test_poll_irq_cleanup_writes_ff_on_timeout();
    test_poll_irq_no_cleanup_does_not_write_on_success();
    test_poll_checks_correct_bit_mask();

    // sx1278_read_last_payload
    test_read_payload_no_rxdone_returns_error();
    test_read_payload_crc_error_returns_error();
    test_read_payload_success();
    test_read_payload_sets_fifo_pointer_before_read();

    // sx1278_send_payload
    test_send_payload_writes_payload_length();
    test_send_payload_enters_tx_mode();
    test_send_payload_switches_to_sleep_after_tx();
    test_send_payload_switches_to_rx_after_tx();

    return 0;
}
