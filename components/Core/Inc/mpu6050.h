/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : mpu6050.h
 * @brief          : Blocking HAL I2C driver for the MPU-6050 IMU.
 ******************************************************************************
 */
/* USER CODE END Header */

#ifndef INC_MPU6050_H_
#define INC_MPU6050_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define MPU6050_I2C_ADDR_LOW  0x68U
#define MPU6050_I2C_ADDR_HIGH 0x69U

typedef struct {
  int16_t accel_x_raw;
  int16_t accel_y_raw;
  int16_t accel_z_raw;
  int16_t temp_raw;
  int16_t gyro_x_raw;
  int16_t gyro_y_raw;
  int16_t gyro_z_raw;

  float accel_x_g;
  float accel_y_g;
  float accel_z_g;
  float temperature_c;
  float gyro_x_dps;
  float gyro_y_dps;
  float gyro_z_dps;
} MPU6050_Data_t;

typedef struct {
  I2C_HandleTypeDef *hi2c;
  uint8_t address;
  float accel_lsb_per_g;
  float gyro_lsb_per_dps;
} MPU6050_HandleTypeDef;

HAL_StatusTypeDef MPU6050_Init(MPU6050_HandleTypeDef *hmpu, I2C_HandleTypeDef *hi2c,
                               uint8_t address);
HAL_StatusTypeDef MPU6050_ReadAll(MPU6050_HandleTypeDef *hmpu, MPU6050_Data_t *data);
HAL_StatusTypeDef MPU6050_ReadDeviceId(MPU6050_HandleTypeDef *hmpu, uint8_t *device_id);

#ifdef __cplusplus
}
#endif

#endif /* INC_MPU6050_H_ */
