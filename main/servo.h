#ifndef SERVO_H
#define SERVO_H

#include <stdbool.h>

void servo_init(void);
void servo_set_on(void);
void servo_set_off(void);
bool servo_is_on(void);

#endif
