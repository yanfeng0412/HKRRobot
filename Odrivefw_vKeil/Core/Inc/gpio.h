/* gpio.h — compatibility shim for MotorControl files ported from ODrive v0.3.6
 *
 * CubeMX 6.x no longer generates a separate gpio.h/gpio.c; all GPIO pin
 * definitions are in main.h.  Including this shim preserves the original
 * includes in the MotorControl files without modification.
 */
#pragma once
#include "main.h"
