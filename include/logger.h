#pragma once

typedef enum
{
    UNINITIALIZED,
    LOG_INFO_LOW, // low priority
    LOG_INFO,
    LOG_INFO_HIGH, // high priority
    LOG_ERROR,

} log_level_t;

typedef void (*logger_t)(char *str, log_level_t log_level);

void set_log_level(log_level_t cfg_log_level);

void configure_logger(logger_t cfg_logger, log_level_t cfg_log_level);

void network_log(char *str, log_level_t param_log_level);
