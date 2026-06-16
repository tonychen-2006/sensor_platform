/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : st7789.h
 * @brief          : ST7789 SPI TFT driver with DMA pixel transfers.
 ******************************************************************************
 */
/* USER CODE END Header */

#ifndef INC_ST7789_H_
#define INC_ST7789_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define ST7789_WIDTH    240U
#define ST7789_HEIGHT   240U
#define ST7789_X_OFFSET 0U
#define ST7789_Y_OFFSET 0U

HAL_StatusTypeDef ST7789_Init(SPI_HandleTypeDef *hspi);
HAL_StatusTypeDef ST7789_FillScreen(uint16_t color);
HAL_StatusTypeDef ST7789_DrawRgb565Dma(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                                       const uint16_t *pixels);
uint8_t ST7789_IsBusy(void);
void ST7789_WaitForDma(void);
uint16_t ST7789_Color565(uint8_t red, uint8_t green, uint8_t blue);

#ifdef __cplusplus
}
#endif

#endif /* INC_ST7789_H_ */
