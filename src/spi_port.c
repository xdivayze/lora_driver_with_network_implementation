#include "spi_port.h"
#include <stdbool.h>

static bool initialized = false;

static spi_register_reader_t spi_burst_reader;
static spi_register_writer_t spi_burst_writer;

void configure_spi_port(spi_register_writer_t cfg_spi_writer, spi_register_reader_t cfg_spi_reader)
{
    spi_burst_reader = cfg_spi_reader;
    spi_burst_writer = cfg_spi_writer;
    initialized = true;
}

//TODO add NULL check for spi pointer
spi_port_return_t spi_burst_write_reg(const void *spi, const uint8_t reg, const uint8_t *data, int len)
{
    if (!initialized)
        return SPI_PORT_UNITIALIZED;
    return spi_burst_writer(spi, reg, data, len);
}

spi_port_return_t spi_burst_read_reg(const void *spi, const uint8_t reg, const uint8_t *data, int len)
{
    if (!initialized)
        return SPI_PORT_UNITIALIZED;
    return spi_burst_reader(spi, reg, data, len);
}