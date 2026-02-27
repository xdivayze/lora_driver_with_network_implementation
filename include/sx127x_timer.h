#pragma once

#include <stdint.h>

//TODO .c file missing
//TODO add tick timer, delayere



typedef int64_t (*sx127x_timer_t)(); //type for function that returns time in micro seconds

void configure_timer(sx127x_timer_t cfg_timer);

int64_t sx127x_timer_get_time_us();