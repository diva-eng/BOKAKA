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
#include "stm32l0xx_hal.h"

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

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define TAP_WAKE_Pin GPIO_PIN_1
#define TAP_WAKE_GPIO_Port GPIOA
#define USR_LED_Pin GPIO_PIN_5
#define USR_LED_GPIO_Port GPIOA
#define BUZZ_PWM_Pin GPIO_PIN_6
#define BUZZ_PWM_GPIO_Port GPIOA
#define LED_EN_Pin GPIO_PIN_7
#define LED_EN_GPIO_Port GPIOA
#define TAP_IO_Pin GPIO_PIN_0
#define TAP_IO_GPIO_Port GPIOB
#define DEBUG_LED_Pin GPIO_PIN_1
#define DEBUG_LED_GPIO_Port GPIOB
#define USR_BTN0_Pin GPIO_PIN_13
#define USR_BTN0_GPIO_Port GPIOB
#define USR_BTN1_Pin GPIO_PIN_14
#define USR_BTN1_GPIO_Port GPIOB
#define USB_DETECT_Pin GPIO_PIN_8
#define USB_DETECT_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
