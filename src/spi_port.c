#include "spi_port.h"

sx127x_err_t spi_port_init(spi_port_t *port, spi_register_writer_t writer, spi_register_reader_t reader, const void *spi_handle)
{
    port->writer = writer;
    port->reader = reader;
    port->spi_handle = spi_handle;
    port->initialized = true;
    return SX_OK;
}

sx127x_err_t spi_burst_write_reg(const spi_port_t *port, uint8_t reg, const uint8_t *data, int len)
{
    if (!port->initialized)
        return SX_UNIT_UNITIALIZED;
    return port->writer(port->spi_handle, reg, data, len);
}

sx127x_err_t spi_burst_read_reg(const spi_port_t *port, uint8_t reg, uint8_t *data, int len)
{
    if (!port->initialized)
        return SX_UNIT_UNITIALIZED;
    return port->reader(port->spi_handle, reg, data, len);
}
