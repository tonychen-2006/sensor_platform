/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : imu_dashboard.h
 * @brief          : Live IMU dashboard renderer for the ST7789 TFT.
 ******************************************************************************
 */
/* USER CODE END Header */

#ifndef INC_IMU_DASHBOARD_H_
#define INC_IMU_DASHBOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "mpu6050.h"
#include "stm32f4xx_hal.h"

HAL_StatusTypeDef IMU_Dashboard_Draw(const MPU6050_Data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* INC_IMU_DASHBOARD_H_ */
