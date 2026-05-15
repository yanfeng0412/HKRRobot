/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * Copyright (c) 2017 STMicroelectronics International N.V. 
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

/* USER CODE BEGIN Includes */     
#include "freertos_vars.h"
#include "low_level.h"
#include "commands.h"
#include "usart.h"   /* huart2 */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

// List of semaphore
osSemaphoreId sem_usb_irq;
osSemaphoreId sem_uart_dma;
osSemaphoreId sem_usb_rx;
osSemaphoreId sem_usb_tx;

// List of threads
osThreadId thread_motor_0;
osThreadId thread_motor_1;
osThreadId thread_cmd_parse;
osThreadId thread_usb_pump;
osThreadId task_packet_timer;

/* Variables -----------------------------------------------------------------*/
osThreadId defaultTaskHandle;

/* USER CODE BEGIN Variables */

/* USER CODE END Variables */

/* Function prototypes -------------------------------------------------------*/
void StartDefaultTask(void const * argument);

extern void MX_USB_DEVICE_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* USER CODE BEGIN FunctionPrototypes */
static void uart2_test_thread(void const *argument);

/* ---- Reporting rate parameters ----------------------------------------- */
/* 115200 baud: ~90-byte frame ≈ 7.8 ms TX time.                            */
/* REPORT_MIN_MS caps Case-1 at ≤100 Hz so UART is never overdriven.        */
#define REPORT_MIN_MS       10u   /* minimum gap between TX frames  [ms]     */
#define IDLE_THRESHOLD_MS  500u   /* no Hall edge → declare idle    [ms]     */
#define REPORT_IDLE_MS    1000u   /* Case-2 periodic interval       [ms]     */

/* USER CODE END FunctionPrototypes */

/* Hook prototypes */

/* Init FreeRTOS */

void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
       
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  // Init usb irq binary semaphore, and start with no tokens by removing the starting one.
  osSemaphoreDef(sem_usb_irq);
  sem_usb_irq = osSemaphoreCreate(osSemaphore(sem_usb_irq), 1);
  osSemaphoreWait(sem_usb_irq, 0);

  // Create a semaphore for UART DMA and remove a token
  osSemaphoreDef(sem_uart_dma);
  sem_uart_dma = osSemaphoreCreate(osSemaphore(sem_uart_dma), 1);

  // Create a semaphore for USB RX
  osSemaphoreDef(sem_usb_rx);
  sem_usb_rx = osSemaphoreCreate(osSemaphore(sem_usb_rx), 1);
  osSemaphoreWait(sem_usb_irq, 0);  // Remove a token.

  // Create a semaphore for USB RX
  osSemaphoreDef(sem_usb_tx);
  sem_usb_tx = osSemaphoreCreate(osSemaphore(sem_usb_tx), 1);

  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
    uint8_t  last_hall_raw = 0xFFu;
  osThreadDef(defaultTask, StartDefaultTask, osPriorityIdle, 0, 256);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */

  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */
}

/* StartDefaultTask function */
void StartDefaultTask(void const * argument)
{
  /* init code for USB_DEVICE */
  MX_USB_DEVICE_Init();

  /* USER CODE BEGIN StartDefaultTask */

  // Init communications
  init_communication();

  // Init motor control
  init_motor_control();

  // Start motor threads
  osThreadDef(task_motor_0, motor_thread,   osPriorityHigh+1, 0, 512);
  osThreadDef(task_motor_1, motor_thread,   osPriorityHigh,   0, 512);
  thread_motor_0 = osThreadCreate(osThread(task_motor_0), &motors[0]);
  thread_motor_1 = osThreadCreate(osThread(task_motor_1), &motors[1]);

  // Start command handling thread
  osThreadDef(task_cmd_parse, cmd_parse_thread, osPriorityNormal, 0, 512);
  thread_cmd_parse = osThreadCreate(osThread(task_cmd_parse), NULL);

  // Start USB interrupt handler thread
  osThreadDef(task_usb_pump, usb_update_thread, osPriorityNormal, 0, 512);
  thread_usb_pump = osThreadCreate(osThread(task_usb_pump), NULL);
	
  // Start USB interrupt handler thread
  osThreadDef(task_packet_timer, packet_timer_thread, osPriorityBelowNormal, 0, 512);
  task_packet_timer = osThreadCreate(osThread(task_packet_timer), NULL);	

  // USART2 heartbeat + command RX on PA2(TX)/PA3(RX) at 115200
  // 512 words: covers snprintf floats + motor_parse_cmd call stack depth
  osThreadDef(task_uart2_test, uart2_test_thread, osPriorityLow, 0, 512);
  osThreadCreate(osThread(task_uart2_test), NULL);

  //If we get to here, then the default task is done.
  vTaskDelete(defaultTaskHandle);

  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Application */

/**
 * vApplicationStackOverflowHook  –  called by FreeRTOS when stack overflow detected.
 * configCHECK_FOR_STACK_OVERFLOW must be 1 or 2 in FreeRTOSConfig.h.
 *
 * In debug: set a breakpoint here, then inspect pxCurrentTCB->pcTaskName
 * or the 'pcTaskName' argument directly.
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    /* pcTaskName = name of the overflowing task (may itself be corrupt if overflow is severe).
     * Add a breakpoint on the next line in the debugger. */
    __disable_irq();
    for (;;) {}  /* Halt – inspect pcTaskName in Watch window */
}

/**
 * uart2_test_thread  –  status TX + command RX on USART2
 * PA2=TX  PA3=RX  115200 8N1
 *
 * RX: interrupt-driven ring buffer (USART2_IRQHandler → uart2_rx_buf).
 *
 * TX reporting modes:
 *   Case 1 – Motion: any Hall edge on M0 or M1 triggers a status frame,
 *             rate-capped at REPORT_MIN_MS to prevent UART TX saturation.
 *             (115200 baud: ~90-byte frame ≈ 7.8 ms TX time, cap=10 ms)
 *             Case 2 is suppressed while motion is active.
 *   Case 2 – Idle:   no Hall change for IDLE_THRESHOLD_MS (500 ms) →
 *             1 Hz periodic report.
 *
 * Frame format (one line, ≤120 chars):
 *
 * TX frame format – positional CSV, same framing as command protocol:
 *   $ST,<vb>,<f>,<M0h>,<M0c>,<M0v>,<M0i>,<M0e>,<M0s>,<M1h>,<M1c>,<M1v>,<M1i>,<M1e>,<M1s>;\r\n
 *
 *   Field   Type    Description
 *   vb      float   VBUS voltage [V]
 *   f       int     DRV8301 fault (0=OK; shared nFAULT, M0 value used)
 *   M0h     uint    M0 Hall state (1-6 valid; 0/7 invalid)
 *   M0c     int     M0 shadow_count (cumulative Hall edge count)
 *   M0v     float   M0 mechanical velocity [rad/s]
 *   M0i     float   M0 Iq measured [A]
 *   M0e     int     M0 error code (0 = no error)
 *   M0s     uint    M0 status: bit0=calibration_ok  bit1=enable_control
 *   M1h..   same fields repeated for M1
 *
 * Parse example (Python):
 *   if line.startswith('$ST,') and line.endswith(';'):
 *       f = line[4:-1].split(',')
 *       vbus=float(f[0]); m0_vel=float(f[4]); m1_vel=float(f[10])
 *
 * Temperature: ADC pins M0_TEMP/M1_TEMP exist but no firmware variable yet.
 */
static void uart2_test_thread(void const *argument) {
    const char *banner =
        "\r\n[BOOT] ODrive 5065 USART2 115200 8N1\r\n"
        "[CMD]  $M0,Q,0;  $M0,V,3.6;  $M0,H,0;  $M0,E,0;\r\n"
        "[FMT]  $ST,vb,f,M0h,M0c,M0v,M0i,M0e,M0s,M1h,M1c,M1v,M1i,M1e,M1s;\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)banner, strlen(banner), 200);

    uint8_t  last_hall0           = 0xFFu;
    uint8_t  last_hall1           = 0xFFu;
    char     rx_buf[64];
    int      rx_idx               = 0;
    bool     in_frame             = false;
    uint32_t last_tx_tick         = 0u;
    uint32_t last_hall_change_tick = 0u;

    for (;;) {
        /* ---- RX: drain ring buffer (filled by USART2_IRQHandler) ---- */
        while (uart2_rx_tail != uart2_rx_head) {
            uint8_t b = uart2_rx_buf[uart2_rx_tail];
            uart2_rx_tail = (uart2_rx_tail + 1U) % UART2_RX_BUF_SIZE;

            if (b == '$') { in_frame = true; rx_idx = 0; continue; }
            if (!in_frame) continue;

            if (b == '\n' || b == '\r') {
                rx_buf[rx_idx] = '\0';

                /* $M0,E,0; or $M1,E,0; – clear error and re-arm motor */
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
                    /* Forward to standard command parser; reply routed to USART2 */
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

        /* is_idle: no Hall edge for IDLE_THRESHOLD_MS */
        bool     is_idle = (now - last_hall_change_tick) >= IDLE_THRESHOLD_MS;
        uint32_t elapsed = now - last_tx_tick;

        bool do_send = false;
        if (!is_idle) {
            /* Case 1: motor moving – fire on Hall edge, rate-limited */
            if (hall_changed && elapsed >= REPORT_MIN_MS)
                do_send = true;
        } else {
            /* Case 2: motor idle – 1 Hz periodic (Case 1 suppressed) */
            if (elapsed >= REPORT_IDLE_MS)
                do_send = true;
        }

        if (do_send) {
            last_tx_tick = now;
            char    buf[160];
            float   vel0 = get_pll_vel(&motors[0]);
            float   vel1 = get_pll_vel(&motors[1]);
            /* status byte: bit0=calibration_ok  bit1=enable_control */
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

        osDelay(1);   /* 1 ms loop; Hall polling resolution */
    }
    vTaskDelete(NULL);
}

/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
