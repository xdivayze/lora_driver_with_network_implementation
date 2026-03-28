#include "logger.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

// --- shared state ---

static char captured_str[4096] = {0};
static log_level_t captured_level = UNINITIALIZED;
static int logger_call_count = 0;

static void reset_state()
{
    captured_str[0] = '\0';
    captured_level = UNINITIALIZED;
    logger_call_count = 0;
}

// --- mock callbacks ---

static void capturing_logger(char *str, log_level_t level)
{
    strncpy(captured_str, str, sizeof(captured_str) - 1);
    captured_level = level;
    logger_call_count++;
}

// --- tests ---

void test_log_at_configured_level()
{
    reset_state();
    logger_ctx_t ctx;
    logger_init(&ctx, capturing_logger, LOG_INFO_LOW);
    network_log(&ctx, "rammus", LOG_INFO_LOW);
    assert(logger_call_count == 1);
    assert(strcmp(captured_str, "rammus") == 0);
}

void test_log_above_configured_level()
{
    reset_state();
    logger_ctx_t ctx;
    logger_init(&ctx, capturing_logger, LOG_INFO_LOW);
    network_log(&ctx, "rammus", LOG_INFO_HIGH);
    assert(logger_call_count == 1);
}

void test_log_below_configured_level_suppressed()
{
    reset_state();
    logger_ctx_t ctx;
    logger_init(&ctx, capturing_logger, LOG_INFO_HIGH);
    network_log(&ctx, "rammus", LOG_INFO_LOW);
    assert(logger_call_count == 0);
}

void test_uninitialized_context_no_op()
{
    reset_state();
    logger_ctx_t ctx = {0};  // log_level == UNINITIALIZED (0)
    network_log(&ctx, "rammus", LOG_INFO_LOW);
    assert(logger_call_count == 0);
}

void test_set_level_dynamically()
{
    reset_state();
    logger_ctx_t ctx;
    logger_init(&ctx, capturing_logger, LOG_INFO_HIGH);

    // below threshold — suppressed
    network_log(&ctx, "msg", LOG_INFO_LOW);
    assert(logger_call_count == 0);

    // lower the threshold
    logger_set_level(&ctx, LOG_INFO_LOW);
    network_log(&ctx, "msg", LOG_INFO_LOW);
    assert(logger_call_count == 1);
}

void test_log_with_tag_formats_correctly()
{
    reset_state();
    logger_ctx_t ctx;
    logger_init(&ctx, capturing_logger, LOG_INFO_LOW);
    network_log_with_tag(&ctx, "DRIVER", "init failed", LOG_INFO_LOW);
    assert(logger_call_count == 1);
    assert(strcmp(captured_str, "DRIVER: init failed") == 0);
}

void test_log_with_tag_passes_correct_level()
{
    reset_state();
    logger_ctx_t ctx;
    logger_init(&ctx, capturing_logger, LOG_INFO_LOW);
    network_log_with_tag(&ctx, "TAG", "msg", LOG_INFO_HIGH);
    assert(captured_level == LOG_INFO_HIGH);
}

void test_log_err_uses_error_level()
{
    reset_state();
    logger_ctx_t ctx;
    logger_init(&ctx, capturing_logger, LOG_INFO_LOW);
    network_log_err(&ctx, "TAG", "something broke");
    assert(logger_call_count == 1);
    assert(captured_level == LOG_ERROR);
    assert(strcmp(captured_str, "TAG: something broke") == 0);
}

void test_log_err_not_suppressed_by_high_threshold()
{
    reset_state();
    logger_ctx_t ctx;
    logger_init(&ctx, capturing_logger, LOG_INFO_HIGH);
    network_log_err(&ctx, "TAG", "critical");
    // LOG_ERROR > LOG_INFO_HIGH so it fires
    assert(logger_call_count == 1);
}

void test_two_contexts_independent()
{
    reset_state();
    logger_ctx_t ctx_low, ctx_high;
    logger_init(&ctx_low,  capturing_logger, LOG_INFO_LOW);
    logger_init(&ctx_high, capturing_logger, LOG_INFO_HIGH);

    // low-priority message goes through ctx_low only
    network_log(&ctx_low,  "msg", LOG_INFO_LOW);
    assert(logger_call_count == 1);

    network_log(&ctx_high, "msg", LOG_INFO_LOW);
    assert(logger_call_count == 1);  // no change — suppressed

    // high-priority message goes through both
    network_log(&ctx_high, "msg", LOG_INFO_HIGH);
    assert(logger_call_count == 2);
}

int main()
{
    test_log_at_configured_level();
    test_log_above_configured_level();
    test_log_below_configured_level_suppressed();
    test_uninitialized_context_no_op();
    test_set_level_dynamically();
    test_log_with_tag_formats_correctly();
    test_log_with_tag_passes_correct_level();
    test_log_err_uses_error_level();
    test_log_err_not_suppressed_by_high_threshold();
    test_two_contexts_independent();
    return 0;
}
