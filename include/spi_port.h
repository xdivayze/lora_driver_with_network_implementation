#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "sx127x_err.h"

typedef sx127x_err_t (*spi_register_writer_t)(const void *spi, const uint8_t reg, const uint8_t *data, int len);
typedef sx127x_err_t (*spi_register_reader_t)(const void *spi, const uint8_t reg, uint8_t *data, int len);

typedef struct {
    spi_register_writer_t writer;
    spi_register_reader_t reader;
    const void *spi_handle;
    bool initialized;
} spi_port_t;

sx127x_err_t spi_port_init(spi_port_t *port, spi_register_writer_t writer, spi_register_reader_t reader, const void *spi_handle);

sx127x_err_t spi_burst_write_reg(const spi_port_t *port, uint8_t reg, const uint8_t *data, int len);

sx127x_err_t spi_burst_read_reg(const spi_port_t *port, uint8_t reg, uint8_t *data, int len);
