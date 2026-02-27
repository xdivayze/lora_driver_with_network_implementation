#include <assert.h>
#include "spi_port.h"
#include <stdlib.h>
#include <string.h>
static int test_switch = 0;
static char test_str[] = "rammus";

sx127x_err_t spi_burst_reader(const void *spi, const uint8_t reg, uint8_t *data, int len)
{
    test_switch = 1;
    return SX_OK;
}

sx127x_err_t spi_burst_writer(const void *spi, const uint8_t reg, const uint8_t *data, int len)
{
    if (strcmp(data, test_str))
    {
        return SX_INVALID_STATE;
    }
    test_switch = 1;
    return SX_OK;
}

int test_config()
{
    configure_spi_port(spi_burst_writer, spi_burst_reader);
    if (spi_burst_write_reg(NULL, 0x00, test_str, sizeof(test_str)) != SX_OK)
        return -1;

    return test_switch == 1 ? 0 : -1;
}

int main()
{
    assert(test_config() == 0);
}