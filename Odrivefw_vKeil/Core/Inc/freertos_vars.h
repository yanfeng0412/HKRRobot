/* =============================================================================
 * freertos_vars.h — extern declarations for FreeRTOS semaphores and task IDs
 *
 * All variables are defined in Core/Src/freertos.c.
 * Include this header wherever semaphores or task IDs are referenced
 * (stm32f4xx_it.c, low_level.c, commands.c, etc.).
 * =============================================================================*/
#ifndef __FREERTOS_VARS_H
#define __FREERTOS_VARS_H

#include "cmsis_os.h"

/* ---- Semaphores ---- */
extern osSemaphoreId sem_usb_irq;    /* USB OTG interrupt -> usb_update_thread     */
extern osSemaphoreId sem_usb_rx;     /* USB RX data ready                          */
extern osSemaphoreId sem_usb_tx;     /* USB TX complete                            */

/* ---- Motor / command task handles ---- */
extern osThreadId thread_motor_0;
extern osThreadId thread_motor_1;
extern osThreadId thread_cmd_parse;
extern osThreadId thread_usb_pump;

/* ---- USART2 status thread (defined in freertos.c) ---- */
void uart2_test_thread(void const *argument);

#endif /* __FREERTOS_VARS_H */
