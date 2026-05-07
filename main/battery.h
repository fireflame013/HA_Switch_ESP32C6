#ifndef BATTERY_H
#define BATTERY_H

#include <stdbool.h>

void battery_init(void);
float battery_read_voltage(void);
bool battery_is_low(void);

#endif
