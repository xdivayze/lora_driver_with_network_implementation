#include "logger.h"
#include <string.h>
#include <stdio.h>
// logs above this level are printed
static log_level_t log_level = UNINITIALIZED;

static logger_t logger;

void set_log_level(log_level_t cfg_log_level)
{
    log_level = cfg_log_level;
}

void configure_logger(logger_t cfg_logger, log_level_t cfg_log_level)
{
    logger = cfg_logger;
    log_level = cfg_log_level;
}

void network_log(char *str, log_level_t param_log_level)
{
    if (log_level == UNINITIALIZED)
        return;

    if (param_log_level >= log_level)
        logger(str, param_log_level);
}

void network_log_with_tag(char* tag, char* str, log_level_t param_log_level) {
    char msg[4096];
    sprintf(msg, "%s: %s", tag, str);
    network_log(msg, param_log_level);
}

void network_log_err(char *tag, char *str)
{
    network_log_with_tag(tag, str, LOG_ERROR);
}