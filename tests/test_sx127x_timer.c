#include <assert.h>
#include "sx127x_timer.h"

// --- shared state ---

static int64_t mock_time_value = 0;
static uint64_t last_delay_arg = 0;
static int delay_call_count = 0;
static int time_call_count = 0;

static void reset_state()
{
    mock_time_value = 0;
    last_delay_arg = 0;
    delay_call_count = 0;
    time_call_count = 0;
}

// --- mock callbacks ---

static int64_t mock_get_time()
{
    time_call_count++;
    return mock_time_value;
}

static void mock_delay(uint64_t ms)
{
    last_delay_arg = ms;
    delay_call_count++;
}

// --- second independent set of mocks ---

static int64_t mock_time_value_b = 0;
static int time_b_call_count = 0;

static int64_t mock_get_time_b()
{
    time_b_call_count++;
    return mock_time_value_b;
}

static void mock_delay_b(uint64_t ms)
{
    (void)ms;
}

// --- tests ---

void test_get_time_returns_callback_value()
{
    reset_state();
    sx127x_timer_ctx_t ctx;
    sx127x_timer_init(&ctx, mock_get_time, mock_delay);

    mock_time_value = 123456;
    int64_t result = sx127x_timer_get_time_us(&ctx);
    assert(result == 123456);
    assert(time_call_count == 1);
}

void test_get_time_reflects_updated_value()
{
    reset_state();
    sx127x_timer_ctx_t ctx;
    sx127x_timer_init(&ctx, mock_get_time, mock_delay);

    mock_time_value = 100;
    assert(sx127x_timer_get_time_us(&ctx) == 100);

    mock_time_value = 9999;
    assert(sx127x_timer_get_time_us(&ctx) == 9999);

    assert(time_call_count == 2);
}

void test_delay_calls_callback()
{
    reset_state();
    sx127x_timer_ctx_t ctx;
    sx127x_timer_init(&ctx, mock_get_time, mock_delay);

    sx127x_task_delay_ms(&ctx, 50);
    assert(delay_call_count == 1);
}

void test_delay_arg_forwarded()
{
    reset_state();
    sx127x_timer_ctx_t ctx;
    sx127x_timer_init(&ctx, mock_get_time, mock_delay);

    sx127x_task_delay_ms(&ctx, 250);
    assert(last_delay_arg == 250);
}

void test_multiple_delays_accumulate()
{
    reset_state();
    sx127x_timer_ctx_t ctx;
    sx127x_timer_init(&ctx, mock_get_time, mock_delay);

    sx127x_task_delay_ms(&ctx, 10);
    sx127x_task_delay_ms(&ctx, 20);
    sx127x_task_delay_ms(&ctx, 30);
    assert(delay_call_count == 3);
    assert(last_delay_arg == 30);
}

void test_two_contexts_independent()
{
    reset_state();
    mock_time_value_b = 0;
    time_b_call_count = 0;

    sx127x_timer_ctx_t ctx_a, ctx_b;
    sx127x_timer_init(&ctx_a, mock_get_time,   mock_delay);
    sx127x_timer_init(&ctx_b, mock_get_time_b, mock_delay_b);

    mock_time_value   = 111;
    mock_time_value_b = 999;

    assert(sx127x_timer_get_time_us(&ctx_a) == 111);
    assert(sx127x_timer_get_time_us(&ctx_b) == 999);

    // each context only called its own callback
    assert(time_call_count   == 1);
    assert(time_b_call_count == 1);
}

void test_zero_delay_forwarded()
{
    reset_state();
    sx127x_timer_ctx_t ctx;
    sx127x_timer_init(&ctx, mock_get_time, mock_delay);

    sx127x_task_delay_ms(&ctx, 0);
    assert(last_delay_arg == 0);
    assert(delay_call_count == 1);
}

int main()
{
    test_get_time_returns_callback_value();
    test_get_time_reflects_updated_value();
    test_delay_calls_callback();
    test_delay_arg_forwarded();
    test_multiple_delays_accumulate();
    test_two_contexts_independent();
    test_zero_delay_forwarded();
    return 0;
}
