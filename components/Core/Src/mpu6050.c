/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : mpu6050.c
 * @brief          : Blocking HAL I2C driver for the MPU-6050 IMU.
 ******************************************************************************
 */
/* USER CODE END Header */

#include "mpu6050.h"

#define MPU6050_REG_SMPLRT_DIV   0x19U
#define MPU6050_REG_CONFIG       0x1AU
#define MPU6050_REG_GYRO_CONFIG  0x1BU
#define MPU6050_REG_ACCEL_CONFIG 0x1CU
#define MPU6050_REG_ACCEL_XOUT_H 0x3BU
#define MPU6050_REG_PWR_MGMT_1   0x6BU
#define MPU6050_REG_WHO_AM_I     0x75U

#define MPU6050_WHO_AM_I_VALUE 0x68U
#define MPU6050_I2C_TIMEOUT_MS 100U

static uint16_t MPU6050_HalAddress(const MPU6050_HandleTypeDef *hmpu);
static int16_t MPU6050_ToInt16(uint8_t msb, uint8_t lsb);
static HAL_StatusTypeDef MPU6050_WriteRegister(MPU6050_HandleTypeDef *hmpu, uint8_t reg,
                                               uint8_t value);
static HAL_StatusTypeDef MPU6050_ReadRegisters(MPU6050_HandleTypeDef *hmpu, uint8_t reg,
                                               uint8_t *data, uint16_t length);

HAL_StatusTypeDef MPU6050_Init(MPU6050_HandleTypeDef *hmpu, I2C_HandleTypeDef *hi2c,
                               uint8_t address) {
  uint8_t device_id = 0U;
  HAL_StatusTypeDef status;

  if ((hmpu == NULL) || (hi2c == NULL)) {
    return HAL_ERROR;
  }

  hmpu->hi2c = hi2c;
  hmpu->address = address;
  hmpu->accel_lsb_per_g = 16384.0f;
  hmpu->gyro_lsb_per_dps = 131.0f;

  status = HAL_I2C_IsDeviceReady(hmpu->hi2c, MPU6050_HalAddress(hmpu), 2U, MPU6050_I2C_TIMEOUT_MS);
  if (status != HAL_OK) {
    return status;
  }

  status = MPU6050_ReadDeviceId(hmpu, &device_id);
  if (status != HAL_OK) {
    return status;
  }

  if (device_id != MPU6050_WHO_AM_I_VALUE) {
    return HAL_ERROR;
  }

  status = MPU6050_WriteRegister(hmpu, MPU6050_REG_PWR_MGMT_1, 0x01U);
  if (status != HAL_OK) {
    return status;
  }

  HAL_Delay(100U);

  status = MPU6050_WriteRegister(hmpu, MPU6050_REG_SMPLRT_DIV, 0x07U);
  if (status != HAL_OK) {
    return status;
  }

  status = MPU6050_WriteRegister(hmpu, MPU6050_REG_CONFIG, 0x03U);
  if (status != HAL_OK) {
    return status;
  }

  status = MPU6050_WriteRegister(hmpu, MPU6050_REG_GYRO_CONFIG, 0x00U);
  if (status != HAL_OK) {
    return status;
  }

  return MPU6050_WriteRegister(hmpu, MPU6050_REG_ACCEL_CONFIG, 0x00U);
}

HAL_StatusTypeDef MPU6050_ReadAll(MPU6050_HandleTypeDef *hmpu, MPU6050_Data_t *data) {
  uint8_t raw[14];
  HAL_StatusTypeDef status;

  if ((hmpu == NULL) || (hmpu->hi2c == NULL) || (data == NULL)) {
    return HAL_ERROR;
  }

  status = MPU6050_ReadRegisters(hmpu, MPU6050_REG_ACCEL_XOUT_H, raw, sizeof(raw));
  if (status != HAL_OK) {
    return status;
  }

  data->accel_x_raw = MPU6050_ToInt16(raw[0], raw[1]);
  data->accel_y_raw = MPU6050_ToInt16(raw[2], raw[3]);
  data->accel_z_raw = MPU6050_ToInt16(raw[4], raw[5]);
  data->temp_raw = MPU6050_ToInt16(raw[6], raw[7]);
  data->gyro_x_raw = MPU6050_ToInt16(raw[8], raw[9]);
  data->gyro_y_raw = MPU6050_ToInt16(raw[10], raw[11]);
  data->gyro_z_raw = MPU6050_ToInt16(raw[12], raw[13]);

  data->accel_x_g = (float)data->accel_x_raw / hmpu->accel_lsb_per_g;
  data->accel_y_g = (float)data->accel_y_raw / hmpu->accel_lsb_per_g;
  data->accel_z_g = (float)data->accel_z_raw / hmpu->accel_lsb_per_g;
  data->temperature_c = ((float)data->temp_raw / 340.0f) + 36.53f;
  data->gyro_x_dps = (float)data->gyro_x_raw / hmpu->gyro_lsb_per_dps;
  data->gyro_y_dps = (float)data->gyro_y_raw / hmpu->gyro_lsb_per_dps;
  data->gyro_z_dps = (float)data->gyro_z_raw / hmpu->gyro_lsb_per_dps;

  return HAL_OK;
}

HAL_StatusTypeDef MPU6050_ReadDeviceId(MPU6050_HandleTypeDef *hmpu, uint8_t *device_id) {
  if ((hmpu == NULL) || (hmpu->hi2c == NULL) || (device_id == NULL)) {
    return HAL_ERROR;
  }

  return MPU6050_ReadRegisters(hmpu, MPU6050_REG_WHO_AM_I, device_id, 1U);
}

static uint16_t MPU6050_HalAddress(const MPU6050_HandleTypeDef *hmpu) {
  return (uint16_t)(hmpu->address << 1);
}

static int16_t MPU6050_ToInt16(uint8_t msb, uint8_t lsb) {
  return (int16_t)((uint16_t)msb << 8 | lsb);
}

static HAL_StatusTypeDef MPU6050_WriteRegister(MPU6050_HandleTypeDef *hmpu, uint8_t reg,
                                               uint8_t value) {
  uint8_t tx_data[2] = {reg, value};

  return HAL_I2C_Master_Transmit(hmpu->hi2c, MPU6050_HalAddress(hmpu), tx_data, sizeof(tx_data),
                                 MPU6050_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef MPU6050_ReadRegisters(MPU6050_HandleTypeDef *hmpu, uint8_t reg,
                                               uint8_t *data, uint16_t length) {
  HAL_StatusTypeDef status;

  status = HAL_I2C_Master_Transmit(hmpu->hi2c, MPU6050_HalAddress(hmpu), &reg, 1U,
                                   MPU6050_I2C_TIMEOUT_MS);
  if (status != HAL_OK) {
    return status;
  }

  return HAL_I2C_Master_Receive(hmpu->hi2c, MPU6050_HalAddress(hmpu), data, length,
                                MPU6050_I2C_TIMEOUT_MS);
}
