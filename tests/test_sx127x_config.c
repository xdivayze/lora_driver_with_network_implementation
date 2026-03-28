#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include "sx127x_config.h"
#include "sx127x_timer.h"
#include "logger.h"
#include "spi_port.h"

// -----------------------------------------------------------------------
// Mock infrastructure
// -----------------------------------------------------------------------

typedef struct { uint8_t reg; uint8_t value; } reg_write_t;

static reg_write_t write_log[512];
static int         write_log_n = 0;
static uint8_t     read_responses[256]; // indexed by register

static void reset_state()
{
    write_log_n = 0;
    memset(write_log, 0, sizeof(write_log));
    memset(read_responses, 0, sizeof(read_responses));
}

static sx127x_err_t recording_writer(const void *spi, const uint8_t reg, const uint8_t *data, int len)
{
    for (int i = 0; i < len && write_log_n < 512; i++)
        write_log[write_log_n++] = (reg_write_t){.reg = reg, .value = data[i]};
    return SX_OK;
}

static sx127x_err_t recording_reader(const void *spi, const uint8_t reg, uint8_t *data, int len)
{
    for (int i = 0; i < len; i++)
        data[i] = read_responses[reg];
    return SX_OK;
}

static int64_t zero_timer()    { return 0; }
static void    noop_delay(uint64_t ms) { (void)ms; }

static void setup()
{
    reset_state();
    spi_port_init(&sx127x_spi_port, recording_writer, recording_reader, NULL);
    sx127x_timer_init(&sx127x_timer_ctx, zero_timer, noop_delay);
    sx127x_logger = (logger_ctx_t){0};
}

// Returns the value of the last write to a register, or 0xFF if not found.
static uint8_t last_write_to(uint8_t reg)
{
    for (int i = write_log_n - 1; i >= 0; i--)
        if (write_log[i].reg == reg)
            return write_log[i].value;
    return 0xFF;
}

// Returns 1 if a write of 'value' to 'reg' exists in the log.
static int wrote(uint8_t reg, uint8_t value)
{
    for (int i = 0; i < write_log_n; i++)
        if (write_log[i].reg == reg && write_log[i].value == value)
            return 1;
    return 0;
}

// Returns the FRF encoded in the last consecutive writes to 0x06, 0x07, 0x08.
static uint32_t capture_frf()
{
    for (int i = 0; i < write_log_n - 2; i++)
        if (write_log[i].reg == 0x06 && write_log[i+1].reg == 0x07 && write_log[i+2].reg == 0x08)
            return ((uint32_t)write_log[i].value   << 16) |
                   ((uint32_t)write_log[i+1].value <<  8) |
                    (uint32_t)write_log[i+2].value;
    return 0;
}

// -----------------------------------------------------------------------
// Channel math tests (no SPI needed)
// -----------------------------------------------------------------------

void test_calculate_channel_num_default_bw()
{
    // (HF - LF - 2*BW) / BW = (865e6 - 863e6 - 250e3) / 125e3 = 14
    assert(calculate_channel_num() == 14);
}

void test_invalid_channel_rejected()
{
    setup();
    size_t n = calculate_channel_num();
    assert(sx_1278_switch_to_nth_channel(n) == SX_INVALID_ARGUMENT);
}

// -----------------------------------------------------------------------
// FRF register encoding tests
// -----------------------------------------------------------------------

#define LF_HZ   863000000.0
#define BW_HZ    125000.0
#define FXOSC_HZ 32000000.0

static void assert_channel_frequency(size_t ch)
{
    setup();
    sx_1278_switch_to_nth_channel(ch);

    uint32_t frf = capture_frf();
    assert(frf != 0);

    double reconstructed = (double)frf * FXOSC_HZ / 524288.0;
    double expected      = LF_HZ + (double)ch * BW_HZ + BW_HZ / 2.0;

    // Allow up to 200 Hz error from integer division in FRF calculation
    assert(fabs(reconstructed - expected) < 200.0);
}

void test_channel_0_frequency() { assert_channel_frequency(0); }
void test_channel_7_frequency() { assert_channel_frequency(7); }
void test_channel_13_frequency(){ assert_channel_frequency(13); }

void test_frf_written_big_endian()
{
    setup();
    sx_1278_switch_to_nth_channel(0);

    // Verify the 3 bytes appear in order MSB→MID→LSB (reg 0x06, 0x07, 0x08)
    int msb_idx = -1, mid_idx = -1, lsb_idx = -1;
    for (int i = 0; i < write_log_n; i++) {
        if (write_log[i].reg == 0x06) msb_idx = i;
        if (write_log[i].reg == 0x07) mid_idx = i;
        if (write_log[i].reg == 0x08) lsb_idx = i;
    }
    assert(msb_idx != -1 && mid_idx != -1 && lsb_idx != -1);
    assert(msb_idx < mid_idx && mid_idx < lsb_idx);
}

// -----------------------------------------------------------------------
// initialize_sx_1278 tests
// -----------------------------------------------------------------------

static sx127x_err_t run_init()
{
    spi_port_t spi;
    spi_port_init(&spi, recording_writer, recording_reader, NULL);
    logger_ctx_t logger = {0};
    sx127x_timer_ctx_t timer;
    sx127x_timer_init(&timer, zero_timer, noop_delay);
    return initialize_sx_1278(spi, logger, timer);
}

void test_initialize_fails_on_wrong_version()
{
    reset_state();
    read_responses[0x42] = 0x00; // wrong version
    assert(run_init() == SX_INVALID_RESPONSE);
}

void test_initialize_fails_on_zero_version()
{
    reset_state();
    read_responses[0x42] = 0xFF; // also wrong
    assert(run_init() == SX_INVALID_RESPONSE);
}

void test_initialize_succeeds_on_correct_version()
{
    reset_state();
    read_responses[0x42] = 0x12; // correct
    assert(run_init() == SX_OK);
}

void test_initialize_writes_pa_config()
{
    reset_state();
    read_responses[0x42] = 0x12;
    run_init();
    // 14 dBm: 0b11111100 = 0xFC to reg 0x09
    assert(wrote(0x09, 0xFC));
}

void test_initialize_writes_ocp()
{
    reset_state();
    read_responses[0x42] = 0x12;
    run_init();
    // 100 mA OCP: 0b00101011 = 0x2B to reg 0x2B
    assert(wrote(0x2B, 0x2B));
}

void test_initialize_writes_sync_word()
{
    reset_state();
    read_responses[0x42] = 0x12;
    run_init();
    // SFD = 0b11010011 = 0xD3 to reg 0x39
    assert(wrote(0x39, 0xD3));
}

void test_initialize_writes_symbol_timeout()
{
    reset_state();
    read_responses[0x42] = 0x12;
    run_init();
    assert(wrote(0x1F, 0xFF));
}

void test_initialize_writes_preamble_length()
{
    reset_state();
    read_responses[0x42] = 0x12;
    run_init();
    // Preamble: MSB=0x00, LSB=0x18 → regs 0x20 and 0x21
    assert(wrote(0x20, 0x00));
    assert(wrote(0x21, 0x18));
}

void test_initialize_zeros_fifo_pointers()
{
    reset_state();
    read_responses[0x42] = 0x12;
    run_init();
    // TX base, RX base, FIFO ptr all set to 0x00
    assert(wrote(0x0D, 0x00));
    assert(wrote(0x0E, 0x00));
    assert(wrote(0x0F, 0x00));
}

int main()
{
    // channel math
    test_calculate_channel_num_default_bw();
    test_invalid_channel_rejected();

    // FRF encoding
    test_channel_0_frequency();
    test_channel_7_frequency();
    test_channel_13_frequency();
    test_frf_written_big_endian();

    // init
    test_initialize_fails_on_wrong_version();
    test_initialize_fails_on_zero_version();
    test_initialize_succeeds_on_correct_version();
    test_initialize_writes_pa_config();
    test_initialize_writes_ocp();
    test_initialize_writes_sync_word();
    test_initialize_writes_symbol_timeout();
    test_initialize_writes_preamble_length();
    test_initialize_zeros_fifo_pointers();

    return 0;
}
