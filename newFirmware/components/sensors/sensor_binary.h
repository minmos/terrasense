#pragma once
#include <stdbool.h>

void sensor_binary_init(void);
void sensor_binary_read(bool *output_states);