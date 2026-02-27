#pragma once

typedef enum {
    SX_OK,
    SX_TIMEOUT,
    SX_INVALID_STATE,
    SX_INVALID_ARGUMENT,
    SX_INVALID_CRC,
    SX_UNIT_UNITIALIZED,
    SX_INVALID_RESPONSE,
    SX_NO_MEM,

} sx127x_err_t;

