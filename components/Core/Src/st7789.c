/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : st7789.c
 * @brief          : ST7789 SPI TFT driver with DMA pixel transfers.
 ******************************************************************************
 */
/* USER CODE END Header */

#include "st7789.h"
#include "main.h"

#define ST7789_CMD_SWRESET 0x01U
#define ST7789_CMD_SLPOUT  0x11U
#define ST7789_CMD_COLMOD  0x3AU
#define ST7789_CMD_MADCTL  0x36U
#define ST7789_CMD_CASET   0x2AU
#define ST7789_CMD_RASET   0x2BU
#define ST7789_CMD_RAMWR   0x2CU
#define ST7789_CMD_INVON   0x21U
#define ST7789_CMD_NORON   0x13U
#define ST7789_CMD_DISPON  0x29U

#define ST7789_SPI_TIMEOUT_MS 100U
#define ST7789_CLEAR_LINES    1U
#define ST7789_DMA_CHUNK_SIZE 60000U

static SPI_HandleTypeDef *st7789_hspi;
static volatile uint8_t st7789_dma_busy;
static const uint8_t *st7789_dma_data;
static uint32_t st7789_dma_bytes_remaining;
static uint16_t st7789_clear_buffer[ST7789_WIDTH * ST7789_CLEAR_LINES];

static void ST7789_Select(void);
static void ST7789_Unselect(void);
static void ST7789_CommandMode(void);
static void ST7789_DataMode(void);
static HAL_StatusTypeDef ST7789_WriteCommand(uint8_t command);
static HAL_StatusTypeDef ST7789_WriteData(const uint8_t *data, uint16_t length);
static HAL_StatusTypeDef ST7789_SetAddressWindow(uint16_t x, uint16_t y, uint16_t width,
                                                 uint16_t height);
static HAL_StatusTypeDef ST7789_StartNextDmaChunk(void);
static HAL_StatusTypeDef ST7789_Reset(void);

HAL_StatusTypeDef ST7789_Init(SPI_HandleTypeDef *hspi) {
  uint8_t color_mode = 0x55U;
  uint8_t madctl = 0x00U;
  HAL_StatusTypeDef status;

  if (hspi == NULL) {
    return HAL_ERROR;
  }

  st7789_hspi = hspi;
  st7789_dma_busy = 0U;
  st7789_dma_data = NULL;
  st7789_dma_bytes_remaining = 0U;
  ST7789_Unselect();
  ST7789_DataMode();

  status = ST7789_Reset();
  if (status != HAL_OK) {
    return status;
  }

  status = ST7789_WriteCommand(ST7789_CMD_SWRESET);
  if (status != HAL_OK) {
    return status;
  }
  HAL_Delay(150U);

  status = ST7789_WriteCommand(ST7789_CMD_SLPOUT);
  if (status != HAL_OK) {
    return status;
  }
  HAL_Delay(120U);

  status = ST7789_WriteCommand(ST7789_CMD_COLMOD);
  if (status != HAL_OK) {
    return status;
  }
  status = ST7789_WriteData(&color_mode, 1U);
  if (status != HAL_OK) {
    return status;
  }

  status = ST7789_WriteCommand(ST7789_CMD_MADCTL);
  if (status != HAL_OK) {
    return status;
  }
  status = ST7789_WriteData(&madctl, 1U);
  if (status != HAL_OK) {
    return status;
  }

  status = ST7789_WriteCommand(ST7789_CMD_INVON);
  if (status != HAL_OK) {
    return status;
  }

  status = ST7789_WriteCommand(ST7789_CMD_NORON);
  if (status != HAL_OK) {
    return status;
  }

  status = ST7789_WriteCommand(ST7789_CMD_DISPON);
  if (status != HAL_OK) {
    return status;
  }
  HAL_Delay(120U);

  return ST7789_FillScreen(ST7789_Color565(0U, 0U, 0U));
}

HAL_StatusTypeDef ST7789_FillScreen(uint16_t color) {
  uint16_t row = 0U;

  if (st7789_hspi == NULL) {
    return HAL_ERROR;
  }

  for (uint32_t i = 0U; i < (uint32_t)(ST7789_WIDTH * ST7789_CLEAR_LINES); i++) {
    st7789_clear_buffer[i] = color;
  }

  while (row < ST7789_HEIGHT) {
    uint16_t lines = ST7789_CLEAR_LINES;

    if ((row + lines) > ST7789_HEIGHT) {
      lines = (uint16_t)(ST7789_HEIGHT - row);
    }

    HAL_StatusTypeDef status =
        ST7789_DrawRgb565Dma(0U, row, ST7789_WIDTH, lines, st7789_clear_buffer);
    if (status != HAL_OK) {
      return status;
    }

    ST7789_WaitForDma();
    row = (uint16_t)(row + lines);
  }

  return HAL_OK;
}

HAL_StatusTypeDef ST7789_DrawRgb565Dma(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                                       const uint16_t *pixels) {
  HAL_StatusTypeDef status;
  uint32_t byte_count = (uint32_t)width * (uint32_t)height * 2U;

  if ((st7789_hspi == NULL) || (pixels == NULL) || (width == 0U) || (height == 0U)) {
    return HAL_ERROR;
  }

  if ((x + width) > ST7789_WIDTH || (y + height) > ST7789_HEIGHT) {
    return HAL_ERROR;
  }

  if (st7789_dma_busy != 0U) {
    return HAL_BUSY;
  }

  status = ST7789_SetAddressWindow(x, y, width, height);
  if (status != HAL_OK) {
    return status;
  }

  ST7789_Select();
  ST7789_DataMode();
  st7789_dma_busy = 1U;
  st7789_dma_data = (const uint8_t *)pixels;
  st7789_dma_bytes_remaining = byte_count;
  status = ST7789_StartNextDmaChunk();
  if (status != HAL_OK) {
    st7789_dma_busy = 0U;
    st7789_dma_data = NULL;
    st7789_dma_bytes_remaining = 0U;
    ST7789_Unselect();
  }

  return status;
}

uint8_t ST7789_IsBusy(void) { return st7789_dma_busy; }

void ST7789_WaitForDma(void) {
  while (st7789_dma_busy != 0U) {
  }
}

uint16_t ST7789_Color565(uint8_t red, uint8_t green, uint8_t blue) {
  uint16_t color = (uint16_t)(((red & 0xF8U) << 8) | ((green & 0xFCU) << 3) | (blue >> 3));

  return (uint16_t)((color << 8) | (color >> 8));
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
  if (hspi == st7789_hspi) {
    if (st7789_dma_bytes_remaining > 0U) {
      if (ST7789_StartNextDmaChunk() != HAL_OK) {
        st7789_dma_busy = 0U;
        st7789_dma_data = NULL;
        st7789_dma_bytes_remaining = 0U;
        ST7789_Unselect();
      }
    } else {
      st7789_dma_busy = 0U;
      st7789_dma_data = NULL;
      ST7789_Unselect();
    }
  }
}

static void ST7789_Select(void) { HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET); }

static void ST7789_Unselect(void) { HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET); }

static void ST7789_CommandMode(void) { HAL_GPIO_WritePin(DC_GPIO_Port, DC_Pin, GPIO_PIN_RESET); }

static void ST7789_DataMode(void) { HAL_GPIO_WritePin(DC_GPIO_Port, DC_Pin, GPIO_PIN_SET); }

static HAL_StatusTypeDef ST7789_WriteCommand(uint8_t command) {
  HAL_StatusTypeDef status;

  ST7789_WaitForDma();
  ST7789_Select();
  ST7789_CommandMode();
  status = HAL_SPI_Transmit(st7789_hspi, &command, 1U, ST7789_SPI_TIMEOUT_MS);
  ST7789_Unselect();

  return status;
}

static HAL_StatusTypeDef ST7789_WriteData(const uint8_t *data, uint16_t length) {
  HAL_StatusTypeDef status;

  ST7789_WaitForDma();
  ST7789_Select();
  ST7789_DataMode();
  status = HAL_SPI_Transmit(st7789_hspi, (uint8_t *)data, length, ST7789_SPI_TIMEOUT_MS);
  ST7789_Unselect();

  return status;
}

static HAL_StatusTypeDef ST7789_SetAddressWindow(uint16_t x, uint16_t y, uint16_t width,
                                                 uint16_t height) {
  uint16_t x_start = (uint16_t)(x + ST7789_X_OFFSET);
  uint16_t x_end = (uint16_t)(x_start + width - 1U);
  uint16_t y_start = (uint16_t)(y + ST7789_Y_OFFSET);
  uint16_t y_end = (uint16_t)(y_start + height - 1U);
  uint8_t data[4];
  HAL_StatusTypeDef status;

  data[0] = (uint8_t)(x_start >> 8);
  data[1] = (uint8_t)x_start;
  data[2] = (uint8_t)(x_end >> 8);
  data[3] = (uint8_t)x_end;
  status = ST7789_WriteCommand(ST7789_CMD_CASET);
  if (status != HAL_OK) {
    return status;
  }
  status = ST7789_WriteData(data, sizeof(data));
  if (status != HAL_OK) {
    return status;
  }

  data[0] = (uint8_t)(y_start >> 8);
  data[1] = (uint8_t)y_start;
  data[2] = (uint8_t)(y_end >> 8);
  data[3] = (uint8_t)y_end;
  status = ST7789_WriteCommand(ST7789_CMD_RASET);
  if (status != HAL_OK) {
    return status;
  }
  status = ST7789_WriteData(data, sizeof(data));
  if (status != HAL_OK) {
    return status;
  }

  return ST7789_WriteCommand(ST7789_CMD_RAMWR);
}

static HAL_StatusTypeDef ST7789_StartNextDmaChunk(void) {
  uint32_t chunk_size = st7789_dma_bytes_remaining;

  if (chunk_size > ST7789_DMA_CHUNK_SIZE) {
    chunk_size = ST7789_DMA_CHUNK_SIZE;
  }

  st7789_dma_bytes_remaining -= chunk_size;
  st7789_dma_data += chunk_size;

  return HAL_SPI_Transmit_DMA(st7789_hspi, (uint8_t *)(st7789_dma_data - chunk_size),
                              (uint16_t)chunk_size);
}

static HAL_StatusTypeDef ST7789_Reset(void) {
  HAL_GPIO_WritePin(RST_GPIO_Port, RST_Pin, GPIO_PIN_RESET);
  HAL_Delay(20U);
  HAL_GPIO_WritePin(RST_GPIO_Port, RST_Pin, GPIO_PIN_SET);
  HAL_Delay(120U);

  return HAL_OK;
}
