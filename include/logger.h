#pragma once

typedef enum
{
    UNINITIALIZED,
    LOG_INFO_LOW,
    LOG_INFO,
    LOG_INFO_HIGH,
    LOG_ERROR,
} log_level_t;

typedef void (*logger_t)(char *str, log_level_t log_level);

typedef struct {
    logger_t logger;
    log_level_t log_level;
} logger_ctx_t;

void logger_init(logger_ctx_t *ctx, logger_t logger, log_level_t log_level);

void logger_set_level(logger_ctx_t *ctx, log_level_t level);

void network_log(logger_ctx_t *ctx, char *str, log_level_t param_log_level);

void network_log_with_tag(logger_ctx_t *ctx, char *tag, char *str, log_level_t param_log_level);

void network_log_err(logger_ctx_t *ctx, char *tag, char *str);
