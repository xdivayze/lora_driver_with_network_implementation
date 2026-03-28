#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef int64_t (*sx127x_timer_t)();
typedef void (*sx127x_task_delayer_ms_t)(uint64_t delay_ms);

typedef struct {
    sx127x_timer_t timer;
    sx127x_task_delayer_ms_t delayer;
    bool configured;
} sx127x_timer_ctx_t;

void sx127x_timer_init(sx127x_timer_ctx_t *ctx, sx127x_timer_t timer, sx127x_task_delayer_ms_t delayer);

int64_t sx127x_timer_get_time_us(const sx127x_timer_ctx_t *ctx);

void sx127x_task_delay_ms(const sx127x_timer_ctx_t *ctx, uint64_t delay_ms);
