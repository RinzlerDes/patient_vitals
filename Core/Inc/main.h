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
#include "stm32wbxx_hal.h"

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

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define VCP_RX_Pin GPIO_PIN_7
#define VCP_RX_GPIO_Port GPIOB
#define T_SWO_Pin GPIO_PIN_3
#define T_SWO_GPIO_Port GPIOB
#define User_B1_Pin GPIO_PIN_12
#define User_B1_GPIO_Port GPIOC
#define User_B1_EXTI_IRQn EXTI15_10_IRQn
#define T_SWDIO_Pin GPIO_PIN_13
#define T_SWDIO_GPIO_Port GPIOA
#define T_SWCLK_Pin GPIO_PIN_14
#define T_SWCLK_GPIO_Port GPIOA
#define I2C3_SCL_Pin GPIO_PIN_13
#define I2C3_SCL_GPIO_Port GPIOB
#define TSC_G4_IO1_Pin GPIO_PIN_6
#define TSC_G4_IO1_GPIO_Port GPIOC
#define VCP_TX_Pin GPIO_PIN_6
#define VCP_TX_GPIO_Port GPIOB
#define User_B2_Pin GPIO_PIN_13
#define User_B2_GPIO_Port GPIOC
#define User_B2_EXTI_IRQn EXTI15_10_IRQn
#define ARD_D4_Pin GPIO_PIN_4
#define ARD_D4_GPIO_Port GPIOE
#define I2C3_SDA_Pin GPIO_PIN_11
#define I2C3_SDA_GPIO_Port GPIOB
#define ARD_D7_Pin GPIO_PIN_2
#define ARD_D7_GPIO_Port GPIOB
#define CS_DISP_Pin GPIO_PIN_0
#define CS_DISP_GPIO_Port GPIOH
#define GPIO_SELECT2_Pin GPIO_PIN_1
#define GPIO_SELECT2_GPIO_Port GPIOH
#define DRDY_Pin GPIO_PIN_1
#define DRDY_GPIO_Port GPIOE
#define ARD_D8_Pin GPIO_PIN_13
#define ARD_D8_GPIO_Port GPIOD
#define ARD_D2_Pin GPIO_PIN_12
#define ARD_D2_GPIO_Port GPIOD
#define INT1_Pin GPIO_PIN_2
#define INT1_GPIO_Port GPIOD
#define INT1_EXTI_IRQn EXTI2_IRQn
#define D_C_DISP_Pin GPIO_PIN_9
#define D_C_DISP_GPIO_Port GPIOC
#define TSC_G4_IO2_Pin GPIO_PIN_7
#define TSC_G4_IO2_GPIO_Port GPIOC
#define INT2_Pin GPIO_PIN_9
#define INT2_GPIO_Port GPIOD
#define INT2_EXTI_IRQn EXTI9_5_IRQn
#define TSC_G6_IO1_Pin GPIO_PIN_10
#define TSC_G6_IO1_GPIO_Port GPIOD
#define GPIO_SELECT1_Pin GPIO_PIN_2
#define GPIO_SELECT1_GPIO_Port GPIOE
#define TSC_G6_IO2_Pin GPIO_PIN_11
#define TSC_G6_IO2_GPIO_Port GPIOD
#define RST_DISP_Pin GPIO_PIN_8
#define RST_DISP_GPIO_Port GPIOC

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
