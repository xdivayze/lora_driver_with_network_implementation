#include "logger.h"
#include <assert.h>
#include <string.h>

char TEST_LOG_STR[] = "rammus";

static int test_switch = 0;

static void logger_fcn(char *str, log_level_t log_level)
{
    if (strcmp(str, TEST_LOG_STR) == 0)
        test_switch = 1;
}

int test_logger_log_level_lower_bound()
{
    logger_ctx_t ctx;
    logger_init(&ctx, logger_fcn, LOG_INFO_LOW);
    network_log(&ctx, TEST_LOG_STR, LOG_INFO_HIGH);
    int ret = test_switch == 1 ? 0 : -1;
    test_switch = 0;
    return ret;
}

int test_logger_log_level_upper_bound()
{
    logger_ctx_t ctx;
    logger_init(&ctx, logger_fcn, LOG_INFO_HIGH);
    network_log(&ctx, TEST_LOG_STR, LOG_INFO_LOW);
    int ret = test_switch == 1 ? 0 : -1;
    test_switch = 0;
    return ret;
}

int test_logger_function_call()
{
    logger_ctx_t ctx;
    logger_init(&ctx, logger_fcn, LOG_INFO_LOW);
    network_log(&ctx, TEST_LOG_STR, LOG_INFO_LOW);
    int ret = test_switch == 1 ? 0 : -1;
    test_switch = 0;
    return ret;
}

int main()
{
    int ret = test_logger_function_call();
    assert(ret == 0);
    ret = test_logger_log_level_upper_bound();
    assert(ret == -1);
    ret = test_logger_log_level_lower_bound();
    assert(ret == 0);
}
