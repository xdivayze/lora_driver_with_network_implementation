#pragma once
#include <stdint.h>

typedef enum
{
    SPI_PORT_UNITIALIZED,
    SPI_PORT_ERROR,
    SPI_PORT_WRITE,
    SPI_PORT_READ,
    SPI_PORT_TIMEOUT,
} spi_port_return_t;

typedef spi_port_return_t (*spi_register_writer_t)(const void *spi, const uint8_t reg, const uint8_t *data, int len);
typedef spi_port_return_t (*spi_register_reader_t)(const void *spi, const uint8_t reg, uint8_t *data, int len);

void configure_spi_port(spi_register_writer_t cfg_spi_writer, spi_register_reader_t cfg_spi_reader);

spi_port_return_t spi_burst_write_reg(const void *spi, const uint8_t reg, const uint8_t *data, int len);

spi_port_return_t spi_burst_read_reg(const void *spi, const uint8_t reg, const uint8_t *data, int len);