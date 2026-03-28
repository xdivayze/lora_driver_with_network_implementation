#include <assert.h>
#include "spi_port.h"
#include <stdlib.h>
#include <string.h>

// --- shared state ---

static int write_called = 0;
static int read_called = 0;
static const void *last_spi_handle = NULL;
static uint8_t last_write_reg = 0;
static uint8_t last_read_reg = 0;
static uint8_t test_data[] = "rammus";

static void reset_state()
{
    write_called = 0;
    read_called = 0;
    last_spi_handle = NULL;
    last_write_reg = 0;
    last_read_reg = 0;
}

// --- mock callbacks ---

static sx127x_err_t mock_writer(const void *spi, const uint8_t reg, const uint8_t *data, int len)
{
    write_called++;
    last_spi_handle = spi;
    last_write_reg = reg;
    return SX_OK;
}

static sx127x_err_t mock_reader(const void *spi, const uint8_t reg, uint8_t *data, int len)
{
    read_called++;
    last_spi_handle = spi;
    last_read_reg = reg;
    return SX_OK;
}

static sx127x_err_t failing_writer(const void *spi, const uint8_t reg, const uint8_t *data, int len)
{
    return SX_INVALID_STATE;
}

static sx127x_err_t failing_reader(const void *spi, const uint8_t reg, uint8_t *data, int len)
{
    return SX_INVALID_STATE;
}

// --- tests ---

void test_write_dispatches()
{
    reset_state();
    spi_port_t port;
    spi_port_init(&port, mock_writer, mock_reader, NULL);
    sx127x_err_t ret = spi_burst_write_reg(&port, 0x01, test_data, sizeof(test_data));
    assert(ret == SX_OK);
    assert(write_called == 1);
    assert(read_called == 0);
}

void test_read_dispatches()
{
    reset_state();
    spi_port_t port;
    uint8_t buf[8];
    spi_port_init(&port, mock_writer, mock_reader, NULL);
    sx127x_err_t ret = spi_burst_read_reg(&port, 0x02, buf, sizeof(buf));
    assert(ret == SX_OK);
    assert(read_called == 1);
    assert(write_called == 0);
}

void test_spi_handle_forwarded_on_write()
{
    reset_state();
    int fake_handle = 99;
    spi_port_t port;
    spi_port_init(&port, mock_writer, mock_reader, &fake_handle);
    spi_burst_write_reg(&port, 0x00, test_data, 1);
    assert(last_spi_handle == &fake_handle);
}

void test_spi_handle_forwarded_on_read()
{
    reset_state();
    int fake_handle = 99;
    spi_port_t port;
    uint8_t buf[1];
    spi_port_init(&port, mock_writer, mock_reader, &fake_handle);
    spi_burst_read_reg(&port, 0x00, buf, 1);
    assert(last_spi_handle == &fake_handle);
}

void test_reg_forwarded_on_write()
{
    reset_state();
    spi_port_t port;
    spi_port_init(&port, mock_writer, mock_reader, NULL);
    spi_burst_write_reg(&port, 0x39, test_data, 1);
    assert(last_write_reg == 0x39);
}

void test_reg_forwarded_on_read()
{
    reset_state();
    spi_port_t port;
    uint8_t buf[1];
    spi_port_init(&port, mock_writer, mock_reader, NULL);
    spi_burst_read_reg(&port, 0x12, buf, 1);
    assert(last_read_reg == 0x12);
}

void test_uninitialized_write_returns_error()
{
    spi_port_t port = {0};
    sx127x_err_t ret = spi_burst_write_reg(&port, 0x00, test_data, 1);
    assert(ret == SX_UNIT_UNITIALIZED);
}

void test_uninitialized_read_returns_error()
{
    spi_port_t port = {0};
    uint8_t buf[1];
    sx127x_err_t ret = spi_burst_read_reg(&port, 0x00, buf, 1);
    assert(ret == SX_UNIT_UNITIALIZED);
}

void test_writer_error_propagated()
{
    spi_port_t port;
    spi_port_init(&port, failing_writer, mock_reader, NULL);
    sx127x_err_t ret = spi_burst_write_reg(&port, 0x00, test_data, 1);
    assert(ret == SX_INVALID_STATE);
}

void test_reader_error_propagated()
{
    spi_port_t port;
    uint8_t buf[1];
    spi_port_init(&port, mock_writer, failing_reader, NULL);
    sx127x_err_t ret = spi_burst_read_reg(&port, 0x00, buf, 1);
    assert(ret == SX_INVALID_STATE);
}

void test_two_ports_independent()
{
    reset_state();
    spi_port_t port_a, port_b;
    spi_port_init(&port_a, mock_writer, mock_reader, NULL);
    spi_port_init(&port_b, failing_writer, failing_reader, NULL);

    assert(spi_burst_write_reg(&port_a, 0x00, test_data, 1) == SX_OK);
    assert(spi_burst_write_reg(&port_b, 0x00, test_data, 1) == SX_INVALID_STATE);
    // only port_a called the tracking mock
    assert(write_called == 1);
}

void test_null_handle_is_forwarded()
{
    reset_state();
    spi_port_t port;
    spi_port_init(&port, mock_writer, mock_reader, NULL);
    spi_burst_write_reg(&port, 0x00, test_data, 1);
    assert(last_spi_handle == NULL);
}

int main()
{
    test_write_dispatches();
    test_read_dispatches();
    test_spi_handle_forwarded_on_write();
    test_spi_handle_forwarded_on_read();
    test_reg_forwarded_on_write();
    test_reg_forwarded_on_read();
    test_uninitialized_write_returns_error();
    test_uninitialized_read_returns_error();
    test_writer_error_propagated();
    test_reader_error_propagated();
    test_two_ports_independent();
    test_null_handle_is_forwarded();
    return 0;
}
