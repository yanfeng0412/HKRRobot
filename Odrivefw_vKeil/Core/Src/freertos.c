/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "freertos_vars.h"
#include "low_level.h"
#include "commands.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* ---- Semaphore handles (extern'd in freertos_vars.h) ---- */
osSemaphoreId sem_usb_irq;
osSemaphoreId sem_usb_rx;
osSemaphoreId sem_usb_tx;

/* ---- Motor / command task handles (extern'd in freertos_vars.h) ---- */
osThreadId thread_motor_0;
osThreadId thread_motor_1;
osThreadId thread_cmd_parse;
osThreadId thread_usb_pump;
osThreadId task_packet_timer;

/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

void uart2_test_thread(void const *argument);

/* ---- Reporting rate parameters ---- */
/* 115200 baud: ~90-byte frame �?? 7.8 ms TX time.                         */
/* REPORT_MIN_MS caps Case-1 at �??100 Hz so UART is never overdriven.    */
#define REPORT_MIN_MS       10u   /* minimum gap between TX frames  [ms]  */
#define IDLE_THRESHOLD_MS  500u   /* no Hall edge �?? declare idle    [ms]  */
#define REPORT_IDLE_MS    1000u   /* Case-2 periodic interval       [ms]  */

/* USER CODE END FunctionPrototypes */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/**
 * vApplicationStackOverflowHook �?? called by FreeRTOS on stack overflow.
 * configCHECK_FOR_STACK_OVERFLOW must be 1 or 2 in FreeRTOSConfig.h.
 * Set a breakpoint here and inspect pcTaskName in the debugger.
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    __disable_irq();
    for (;;) {}
}

/**
 * uart2_test_thread  �??  status TX + command RX on USART2
 * PA2=TX  PA3=RX  115200 8N1
 *
 * RX: interrupt-driven ring buffer (USART2_IRQHandler �?? uart2_rx_buf).
 *
 * TX modes:
 *   Case 1 �?? Motion: Hall edge on M0 or M1 �?? status frame,
 *             rate-capped at REPORT_MIN_MS (10 ms).
 *   Case 2 �?? Idle: no Hall change for IDLE_THRESHOLD_MS (500 ms) �??
 *             1 Hz periodic report.
 *
 * Frame format:
 *   $ST,<vb>,<f>,<M0h>,<M0c>,<M0v>,<M0i>,<M0e>,<M0s>,
 *            <M1h>,<M1c>,<M1v>,<M1i>,<M1e>,<M1s>;\r\n
 *
 *   vb   float  VBUS voltage [V]
 *   f    int    DRV8301 nFAULT (0=OK; M0 value used for shared pin)
 *   M#h  uint   Hall state (1-6 valid, 0/7 invalid)
 *   M#c  long   shadow_count (cumulative Hall edge count)
 *   M#v  float  mechanical velocity [rad/s]
 *   M#i  float  Iq_measured [A]
 *   M#e  int    error code  (0 = no error)
 *   M#s  uint   status: bit0=calibration_ok  bit1=enable_control
 */
void uart2_test_thread(void const *argument) {
    const char *banner =
        "\r\n[BOOT] ODrive 5065 USART2 115200 8N1\r\n"
        "[CMD]  $M0,Q,0;  $M0,V,3.6;  $M0,H,0;  $M0,E,0;\r\n"
        "[FMT]  $ST,vb,f,M0h,M0c,M0v,M0i,M0e,M0s,M1h,M1c,M1v,M1i,M1e,M1s;\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)banner, strlen(banner), 200);

    uint8_t  last_hall0            = 0xFFu;
    uint8_t  last_hall1            = 0xFFu;
    char     rx_buf[64];
    int      rx_idx                = 0;
    bool     in_frame              = false;
    uint32_t last_tx_tick          = 0u;
    uint32_t last_hall_change_tick = 0u;

    for (;;) {
        /* ---- RX: drain ring buffer (filled by USART2_IRQHandler) ---- */
        while (uart2_rx_tail != uart2_rx_head) {
            uint8_t b = uart2_rx_buf[uart2_rx_tail];
            uart2_rx_tail = (uart2_rx_tail + 1U) % UART2_RX_BUF_SIZE;

            if (b == '$') { in_frame = true; rx_idx = 0; continue; }
            if (!in_frame) continue;

            if (b == ';') {
                rx_buf[rx_idx] = '\0';
                /* $M0,E,0 / $M1,E,0 �?? clear error and re-arm motor */
                if (rx_idx >= 5 && rx_buf[3] == 'E') {
                    unsigned midx = (rx_buf[1] == '1') ? 1u : 0u;
                    if (midx < num_motors) {
                        motors[midx].error          = ERROR_NO_ERROR;
                        motors[midx].calibration_ok = false;
                        motors[midx].do_calibration = false;
                        motors[midx].enable_control = true;
                        const char *ack = "[OK,err_cleared]\r\n";
                        HAL_UART_Transmit(&huart2, (uint8_t*)ack, strlen(ack), 50);
                    }
                } else {
                    /* Forward to standard command parser */
                    motor_parse_cmd((uint8_t*)rx_buf, rx_idx + 1, SERIAL_PRINTF_IS_UART2);
                }
                in_frame = false; rx_idx = 0;
            } else if (rx_idx < (int)sizeof(rx_buf) - 2) {
                rx_buf[rx_idx++] = (char)b;
            } else {
                in_frame = false; rx_idx = 0;  /* overflow: discard frame */
            }
        }

        /* ---- TX: status reporting ---- */
        uint32_t now   = osKernelSysTick();
        uint8_t  hall0 = read_hall_gpio_state(&motors[0].hall);
        uint8_t  hall1 = read_hall_gpio_state(&motors[1].hall);

        bool hall_changed = (hall0 != last_hall0) || (hall1 != last_hall1);
        if (hall_changed) {
            last_hall0 = hall0;
            last_hall1 = hall1;
            last_hall_change_tick = now;
        }

        bool     is_idle = (now - last_hall_change_tick) >= IDLE_THRESHOLD_MS;
        uint32_t elapsed = now - last_tx_tick;

        bool do_send = false;
        if (!is_idle) {
            if (hall_changed && elapsed >= REPORT_MIN_MS)
                do_send = true;
        } else {
            if (elapsed >= REPORT_IDLE_MS)
                do_send = true;
        }

        if (do_send) {
            last_tx_tick = now;
            char    buf[160];
            float   vel0 = get_pll_vel(&motors[0]);
            float   vel1 = get_pll_vel(&motors[1]);
            uint8_t st0  = (uint8_t)((motors[0].calibration_ok ? 1u : 0u) |
                                     (motors[0].enable_control  ? 2u : 0u));
            uint8_t st1  = (uint8_t)((motors[1].calibration_ok ? 1u : 0u) |
                                     (motors[1].enable_control  ? 2u : 0u));
            int len = snprintf(buf, sizeof(buf),
                "$ST,%.1f,%d,%u,%ld,%.1f,%.2f,%d,%u,%u,%ld,%.1f,%.2f,%d,%u;\r\n",
                vbus_voltage,
                (int)motors[0].drv_fault,
                (unsigned)hall0,
                (long)motors[0].hall.shadow_count,
                vel0,
                motors[0].current_control.Iq_measured,
                (int)motors[0].error,
                (unsigned)st0,
                (unsigned)hall1,
                (long)motors[1].hall.shadow_count,
                vel1,
                motors[1].current_control.Iq_measured,
                (int)motors[1].error,
                (unsigned)st1);
            if (len > 0 && len < (int)sizeof(buf))
                HAL_UART_Transmit(&huart2, (uint8_t*)buf, (uint16_t)len, 20);
        }

        osDelay(1);
    }
    vTaskDelete(NULL);
}

/* USER CODE END Application */
