#include "sx127x_timer.h"

void sx127x_timer_init(sx127x_timer_ctx_t *ctx, sx127x_timer_t timer, sx127x_task_delayer_ms_t delayer)
{
    ctx->timer = timer;
    ctx->delayer = delayer;
    ctx->configured = true;
}

int64_t sx127x_timer_get_time_us(const sx127x_timer_ctx_t *ctx)
{
    return ctx->timer();
}

void sx127x_task_delay_ms(const sx127x_timer_ctx_t *ctx, uint64_t delay_ms)
{
    ctx->delayer(delay_ms);
}
