/* usart.h — compatibility shim for MotorControl files ported from ODrive v0.3.6
 *
 * CubeMX 6.x no longer generates a separate usart.h/usart.c; all UART handles
 * are declared in main.h.  Including this shim preserves the original includes
 * in the MotorControl files without modification.
 */
#pragma once
#include "main.h"
