#pragma once

#include <stdint.h>

// TODO .c file missing

typedef int64_t (*sx127x_timer_t)();                      // type for function that returns time in micro seconds
typedef void(*sx127x_task_delay_ms_t(uint64_t delay_ms)); // program sleeper

void configure_timer(sx127x_timer_t cfg_timer, sx127x_task_delay_ms_t cfg_delayer);

int64_t sx127x_timer_get_time_us();
void sx127x_task_delay_ms(uint64_t delay_ms);