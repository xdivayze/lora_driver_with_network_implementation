#include "logger.h"
#include <string.h>
#include <stdio.h>

void logger_init(logger_ctx_t *ctx, logger_t logger, log_level_t log_level)
{
    ctx->logger = logger;
    ctx->log_level = log_level;
}

void logger_set_level(logger_ctx_t *ctx, log_level_t level)
{
    ctx->log_level = level;
}

void network_log(logger_ctx_t *ctx, char *str, log_level_t param_log_level)
{
    if (ctx->log_level == UNINITIALIZED)
        return;

    if (param_log_level >= ctx->log_level)
        ctx->logger(str, param_log_level);
}

void network_log_with_tag(logger_ctx_t *ctx, char *tag, char *str, log_level_t param_log_level)
{
    char msg[4096];
    sprintf(msg, "%s: %s", tag, str);
    network_log(ctx, msg, param_log_level);
}

void network_log_err(logger_ctx_t *ctx, char *tag, char *str)
{
    network_log_with_tag(ctx, tag, str, LOG_ERROR);
}
