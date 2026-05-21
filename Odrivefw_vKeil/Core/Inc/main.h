/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
extern UART_HandleTypeDef  huart2,huart4;
extern TIM_HandleTypeDef   htim1, htim2, htim3, htim4, htim8;
extern SPI_HandleTypeDef   hspi3;
extern ADC_HandleTypeDef   hadc1, hadc2, hadc3;
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define M0_nCS_Pin GPIO_PIN_13
#define M0_nCS_GPIO_Port GPIOC
#define M1_nCS_Pin GPIO_PIN_14
#define M1_nCS_GPIO_Port GPIOC
#define M1_ENCZ_Pin GPIO_PIN_15
#define M1_ENCZ_GPIO_Port GPIOC
#define M0_IB_Pin GPIO_PIN_0
#define M0_IB_GPIO_Port GPIOC
#define M0_IC_Pin GPIO_PIN_1
#define M0_IC_GPIO_Port GPIOC
#define M1_IC_Pin GPIO_PIN_2
#define M1_IC_GPIO_Port GPIOC
#define M1_IB_Pin GPIO_PIN_3
#define M1_IB_GPIO_Port GPIOC
#define M1_TEMP_Pin GPIO_PIN_4
#define M1_TEMP_GPIO_Port GPIOA
#define AUX_I_Pin GPIO_PIN_5
#define AUX_I_GPIO_Port GPIOA
#define VBUS_S_Pin GPIO_PIN_6
#define VBUS_S_GPIO_Port GPIOA
#define M1_AL_Pin GPIO_PIN_7
#define M1_AL_GPIO_Port GPIOA
#define AUX_TEMP_Pin GPIO_PIN_4
#define AUX_TEMP_GPIO_Port GPIOC
#define M0_TEMP_Pin GPIO_PIN_5
#define M0_TEMP_GPIO_Port GPIOC
#define M1_BL_Pin GPIO_PIN_0
#define M1_BL_GPIO_Port GPIOB
#define M1_CL_Pin GPIO_PIN_1
#define M1_CL_GPIO_Port GPIOB
#define AUX_L_Pin GPIO_PIN_10
#define AUX_L_GPIO_Port GPIOB
#define AUX_H_Pin GPIO_PIN_11
#define AUX_H_GPIO_Port GPIOB
#define EN_GATE_Pin GPIO_PIN_12
#define EN_GATE_GPIO_Port GPIOB
#define M0_AL_Pin GPIO_PIN_13
#define M0_AL_GPIO_Port GPIOB
#define M0_BL_Pin GPIO_PIN_14
#define M0_BL_GPIO_Port GPIOB
#define M0_CL_Pin GPIO_PIN_15
#define M0_CL_GPIO_Port GPIOB
#define M1_AH_Pin GPIO_PIN_6
#define M1_AH_GPIO_Port GPIOC
#define M1_BH_Pin GPIO_PIN_7
#define M1_BH_GPIO_Port GPIOC
#define M1_CH_Pin GPIO_PIN_8
#define M1_CH_GPIO_Port GPIOC
#define M0_ENCZ_Pin GPIO_PIN_9
#define M0_ENCZ_GPIO_Port GPIOC
#define M0_AH_Pin GPIO_PIN_8
#define M0_AH_GPIO_Port GPIOA
#define M0_BH_Pin GPIO_PIN_9
#define M0_BH_GPIO_Port GPIOA
#define M0_CH_Pin GPIO_PIN_10
#define M0_CH_GPIO_Port GPIOA
#define DRV_SCK_Pin GPIO_PIN_10
#define DRV_SCK_GPIO_Port GPIOC
#define DRV_MISO_Pin GPIO_PIN_11
#define DRV_MISO_GPIO_Port GPIOC
#define DRV_MOSI_Pin GPIO_PIN_12
#define DRV_MOSI_GPIO_Port GPIOC
#define nFAULT_Pin GPIO_PIN_2
#define nFAULT_GPIO_Port GPIOD
#define M0_ENCA_Pin GPIO_PIN_4
#define M0_ENCA_GPIO_Port GPIOB
#define M0_ENCB_Pin GPIO_PIN_5
#define M0_ENCB_GPIO_Port GPIOB
#define M1_ENCA_Pin GPIO_PIN_6
#define M1_ENCA_GPIO_Port GPIOB
#define M1_ENCB_Pin GPIO_PIN_7
#define M1_ENCB_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#include "app_config.h"

/* USART2 ring buffer (filled by USART2_IRQHandler, drained by uart2_test_thread) */
#define UART2_RX_BUF_SIZE       64U
extern volatile uint8_t  uart2_rx_buf[UART2_RX_BUF_SIZE];
extern volatile uint32_t uart2_rx_head;   /* written by IRQ  */
extern volatile uint32_t uart2_rx_tail;   /* read  by task   */
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
