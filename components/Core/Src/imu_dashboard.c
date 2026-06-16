/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : imu_dashboard.c
 * @brief          : Live IMU dashboard renderer for the ST7789 TFT.
 ******************************************************************************
 */
/* USER CODE END Header */

#include "imu_dashboard.h"
#include "st7789.h"

#define IMU_DASHBOARD_WIDTH    ST7789_WIDTH
#define IMU_DASHBOARD_HEIGHT   ST7789_HEIGHT
#define IMU_DASHBOARD_X_OFFSET 0U
#define IMU_DASHBOARD_Y_OFFSET 0U
#define IMU_DASHBOARD_PLOT_BOTTOM 176
#define IMU_DASHBOARD_ORIGIN_X    120
#define IMU_DASHBOARD_ORIGIN_Y    92
#define IMU_DASHBOARD_GRID_STEP   24
#define IMU_DASHBOARD_G_STEP      48

typedef struct {
  int16_t x;
  int16_t y;
} Point2D_t;

static uint16_t imu_framebuffer[IMU_DASHBOARD_WIDTH * IMU_DASHBOARD_HEIGHT];

static uint16_t color_background;
static uint16_t color_grid;
static uint16_t color_body_edge;
static uint16_t color_text;
static uint16_t color_x_axis;
static uint16_t color_y_axis;
static uint16_t color_z_axis;

static void IMU_Dashboard_InitColors(void);
static void IMU_Dashboard_Clear(uint16_t color);
static void IMU_Dashboard_DrawPixel(int16_t x, int16_t y, uint16_t color);
static void IMU_Dashboard_DrawLine(Point2D_t p0, Point2D_t p1, uint16_t color);
static void IMU_Dashboard_DrawThickLine(Point2D_t p0, Point2D_t p1, uint16_t color);
static void IMU_Dashboard_DrawRect(int16_t x, int16_t y, int16_t width, int16_t height,
                                   uint16_t color);
static void IMU_Dashboard_DrawCircle(int16_t cx, int16_t cy, int16_t radius, uint16_t color);
static void IMU_Dashboard_FillCircle(int16_t cx, int16_t cy, int16_t radius, uint16_t color);
static void IMU_Dashboard_DrawGrid(void);
static void IMU_Dashboard_DrawOriginView(const MPU6050_Data_t *data);
static void IMU_Dashboard_DrawAxis(Point2D_t start, Point2D_t end, uint16_t color);
static void IMU_Dashboard_DrawArrowHead(Point2D_t start, Point2D_t end, uint16_t color);
static void IMU_Dashboard_DrawReadouts(const MPU6050_Data_t *data);
static void IMU_Dashboard_DrawText(int16_t x, int16_t y, const char *text, uint16_t color);
static void IMU_Dashboard_DrawChar(int16_t x, int16_t y, char value, uint16_t color);
static const uint8_t *IMU_Dashboard_Glyph(char value);
static int16_t IMU_Dashboard_ClampInt16(int16_t value, int16_t min, int16_t max);
static int16_t IMU_Dashboard_FloatToScaled(float value, float scale, int16_t min, int16_t max);
static void IMU_Dashboard_FormatCenti(char *buffer, const char *label, float value);
static void IMU_Dashboard_FormatInteger(char *buffer, const char *label, float value);

HAL_StatusTypeDef IMU_Dashboard_Draw(const MPU6050_Data_t *data) {
  if (data == NULL) {
    return HAL_ERROR;
  }

  if (ST7789_IsBusy() != 0U) {
    return HAL_BUSY;
  }

  IMU_Dashboard_InitColors();
  IMU_Dashboard_Clear(color_background);
  IMU_Dashboard_DrawGrid();
  IMU_Dashboard_DrawOriginView(data);
  IMU_Dashboard_DrawReadouts(data);

  return ST7789_DrawRgb565Dma(IMU_DASHBOARD_X_OFFSET, IMU_DASHBOARD_Y_OFFSET, IMU_DASHBOARD_WIDTH,
                              IMU_DASHBOARD_HEIGHT, imu_framebuffer);
}

static void IMU_Dashboard_InitColors(void) {
  static uint8_t initialized = 0U;

  if (initialized != 0U) {
    return;
  }

  color_background = ST7789_Color565(0U, 0U, 0U);
  color_grid = ST7789_Color565(14U, 24U, 30U);
  color_body_edge = ST7789_Color565(142U, 158U, 164U);
  color_text = ST7789_Color565(206U, 225U, 219U);
  color_x_axis = ST7789_Color565(235U, 53U, 64U);
  color_y_axis = ST7789_Color565(50U, 218U, 113U);
  color_z_axis = ST7789_Color565(60U, 118U, 255U);
  initialized = 1U;
}

static void IMU_Dashboard_Clear(uint16_t color) {
  for (uint32_t i = 0U; i < (uint32_t)(IMU_DASHBOARD_WIDTH * IMU_DASHBOARD_HEIGHT); i++) {
    imu_framebuffer[i] = color;
  }
}

static void IMU_Dashboard_DrawPixel(int16_t x, int16_t y, uint16_t color) {
  if ((x < 0) || (y < 0) || (x >= (int16_t)IMU_DASHBOARD_WIDTH) ||
      (y >= (int16_t)IMU_DASHBOARD_HEIGHT)) {
    return;
  }

  imu_framebuffer[(uint32_t)y * IMU_DASHBOARD_WIDTH + (uint32_t)x] = color;
}

static void IMU_Dashboard_DrawLine(Point2D_t p0, Point2D_t p1, uint16_t color) {
  int16_t dx = (p1.x > p0.x) ? (p1.x - p0.x) : (p0.x - p1.x);
  int16_t sx = (p0.x < p1.x) ? 1 : -1;
  int16_t dy = (p1.y > p0.y) ? (p0.y - p1.y) : (p1.y - p0.y);
  int16_t sy = (p0.y < p1.y) ? 1 : -1;
  int16_t error = (int16_t)(dx + dy);

  while (1) {
    IMU_Dashboard_DrawPixel(p0.x, p0.y, color);

    if ((p0.x == p1.x) && (p0.y == p1.y)) {
      break;
    }

    int16_t twice_error = (int16_t)(2 * error);
    if (twice_error >= dy) {
      error = (int16_t)(error + dy);
      p0.x = (int16_t)(p0.x + sx);
    }
    if (twice_error <= dx) {
      error = (int16_t)(error + dx);
      p0.y = (int16_t)(p0.y + sy);
    }
  }
}

static void IMU_Dashboard_DrawThickLine(Point2D_t p0, Point2D_t p1, uint16_t color) {
  IMU_Dashboard_DrawLine(p0, p1, color);
  IMU_Dashboard_DrawLine((Point2D_t){(int16_t)(p0.x + 1), p0.y},
                         (Point2D_t){(int16_t)(p1.x + 1), p1.y}, color);
  IMU_Dashboard_DrawLine((Point2D_t){p0.x, (int16_t)(p0.y + 1)},
                         (Point2D_t){p1.x, (int16_t)(p1.y + 1)}, color);
}

static void IMU_Dashboard_DrawRect(int16_t x, int16_t y, int16_t width, int16_t height,
                                   uint16_t color) {
  Point2D_t p0 = {x, y};
  Point2D_t p1 = {(int16_t)(x + width - 1), y};
  Point2D_t p2 = {(int16_t)(x + width - 1), (int16_t)(y + height - 1)};
  Point2D_t p3 = {x, (int16_t)(y + height - 1)};

  IMU_Dashboard_DrawLine(p0, p1, color);
  IMU_Dashboard_DrawLine(p1, p2, color);
  IMU_Dashboard_DrawLine(p2, p3, color);
  IMU_Dashboard_DrawLine(p3, p0, color);
}

static void IMU_Dashboard_DrawCircle(int16_t cx, int16_t cy, int16_t radius, uint16_t color) {
  int16_t x = radius;
  int16_t y = 0;
  int16_t error = 0;

  while (x >= y) {
    IMU_Dashboard_DrawPixel((int16_t)(cx + x), (int16_t)(cy + y), color);
    IMU_Dashboard_DrawPixel((int16_t)(cx + y), (int16_t)(cy + x), color);
    IMU_Dashboard_DrawPixel((int16_t)(cx - y), (int16_t)(cy + x), color);
    IMU_Dashboard_DrawPixel((int16_t)(cx - x), (int16_t)(cy + y), color);
    IMU_Dashboard_DrawPixel((int16_t)(cx - x), (int16_t)(cy - y), color);
    IMU_Dashboard_DrawPixel((int16_t)(cx - y), (int16_t)(cy - x), color);
    IMU_Dashboard_DrawPixel((int16_t)(cx + y), (int16_t)(cy - x), color);
    IMU_Dashboard_DrawPixel((int16_t)(cx + x), (int16_t)(cy - y), color);

    y++;
    if (error <= 0) {
      error = (int16_t)(error + 2 * y + 1);
    }
    if (error > 0) {
      x--;
      error = (int16_t)(error - 2 * x + 1);
    }
  }
}

static void IMU_Dashboard_FillCircle(int16_t cx, int16_t cy, int16_t radius, uint16_t color) {
  int16_t radius_sq = (int16_t)(radius * radius);

  for (int16_t y = (int16_t)-radius; y <= radius; y++) {
    for (int16_t x = (int16_t)-radius; x <= radius; x++) {
      if ((x * x + y * y) <= radius_sq) {
        IMU_Dashboard_DrawPixel((int16_t)(cx + x), (int16_t)(cy + y), color);
      }
    }
  }
}

static void IMU_Dashboard_DrawGrid(void) {
  for (int16_t x = IMU_DASHBOARD_ORIGIN_X; x >= 0; x = (int16_t)(x - IMU_DASHBOARD_GRID_STEP)) {
    IMU_Dashboard_DrawLine((Point2D_t){x, 0}, (Point2D_t){x, IMU_DASHBOARD_PLOT_BOTTOM},
                           color_grid);
  }

  for (int16_t x = (int16_t)(IMU_DASHBOARD_ORIGIN_X + IMU_DASHBOARD_GRID_STEP);
       x < (int16_t)IMU_DASHBOARD_WIDTH; x = (int16_t)(x + IMU_DASHBOARD_GRID_STEP)) {
    IMU_Dashboard_DrawLine((Point2D_t){x, 0}, (Point2D_t){x, IMU_DASHBOARD_PLOT_BOTTOM},
                           color_grid);
  }

  for (int16_t y = IMU_DASHBOARD_ORIGIN_Y; y >= 0; y = (int16_t)(y - IMU_DASHBOARD_GRID_STEP)) {
    IMU_Dashboard_DrawLine((Point2D_t){0, y}, (Point2D_t){239, y}, color_grid);
  }

  for (int16_t y = (int16_t)(IMU_DASHBOARD_ORIGIN_Y + IMU_DASHBOARD_GRID_STEP);
       y < IMU_DASHBOARD_PLOT_BOTTOM; y = (int16_t)(y + IMU_DASHBOARD_GRID_STEP)) {
    IMU_Dashboard_DrawLine((Point2D_t){0, y}, (Point2D_t){239, y}, color_grid);
  }

  IMU_Dashboard_DrawLine((Point2D_t){0, IMU_DASHBOARD_ORIGIN_Y},
                         (Point2D_t){239, IMU_DASHBOARD_ORIGIN_Y}, color_body_edge);
  IMU_Dashboard_DrawLine((Point2D_t){IMU_DASHBOARD_ORIGIN_X, 0},
                         (Point2D_t){IMU_DASHBOARD_ORIGIN_X, IMU_DASHBOARD_PLOT_BOTTOM},
                         color_body_edge);

  for (int16_t offset = (int16_t)-IMU_DASHBOARD_G_STEP; offset <= IMU_DASHBOARD_G_STEP;
       offset = (int16_t)(offset + IMU_DASHBOARD_G_STEP)) {
    int16_t x_tick = (int16_t)(IMU_DASHBOARD_ORIGIN_X + offset);
    int16_t y_tick = (int16_t)(IMU_DASHBOARD_ORIGIN_Y + offset);

    IMU_Dashboard_DrawLine((Point2D_t){x_tick, (int16_t)(IMU_DASHBOARD_ORIGIN_Y - 4)},
                           (Point2D_t){x_tick, (int16_t)(IMU_DASHBOARD_ORIGIN_Y + 4)},
                           color_body_edge);
    IMU_Dashboard_DrawLine((Point2D_t){(int16_t)(IMU_DASHBOARD_ORIGIN_X - 4), y_tick},
                           (Point2D_t){(int16_t)(IMU_DASHBOARD_ORIGIN_X + 4), y_tick},
                           color_body_edge);
  }

  IMU_Dashboard_DrawText((int16_t)(IMU_DASHBOARD_ORIGIN_X + IMU_DASHBOARD_G_STEP - 8),
                         (int16_t)(IMU_DASHBOARD_ORIGIN_Y + 8), "+1G", color_body_edge);
  IMU_Dashboard_DrawText((int16_t)(IMU_DASHBOARD_ORIGIN_X - IMU_DASHBOARD_G_STEP - 14),
                         (int16_t)(IMU_DASHBOARD_ORIGIN_Y + 8), "-1G", color_body_edge);
  IMU_Dashboard_DrawText((int16_t)(IMU_DASHBOARD_ORIGIN_X + 8),
                         (int16_t)(IMU_DASHBOARD_ORIGIN_Y - IMU_DASHBOARD_G_STEP - 3), "+1G",
                         color_body_edge);
  IMU_Dashboard_DrawText((int16_t)(IMU_DASHBOARD_ORIGIN_X + 8),
                         (int16_t)(IMU_DASHBOARD_ORIGIN_Y + IMU_DASHBOARD_G_STEP - 10), "-1G",
                         color_body_edge);

  IMU_Dashboard_DrawRect(0, 0, IMU_DASHBOARD_WIDTH, IMU_DASHBOARD_HEIGHT, color_grid);
  IMU_Dashboard_DrawLine((Point2D_t){8, IMU_DASHBOARD_PLOT_BOTTOM},
                         (Point2D_t){231, IMU_DASHBOARD_PLOT_BOTTOM}, color_grid);
}

static void IMU_Dashboard_DrawOriginView(const MPU6050_Data_t *data) {
  int16_t cx = IMU_DASHBOARD_ORIGIN_X;
  int16_t cy = IMU_DASHBOARD_ORIGIN_Y;
  int16_t roll = IMU_Dashboard_FloatToScaled(data->accel_y_g, 28.0f, -32, 32);
  int16_t pitch = IMU_Dashboard_FloatToScaled(data->accel_x_g, 24.0f, -28, 28);
  int16_t z_depth = IMU_Dashboard_FloatToScaled(data->accel_z_g - 1.0f, 20.0f, -16, 16);
  int16_t accel_grid_x = IMU_Dashboard_FloatToScaled(data->accel_y_g, IMU_DASHBOARD_G_STEP,
                                                     -IMU_DASHBOARD_G_STEP, IMU_DASHBOARD_G_STEP);
  int16_t accel_grid_y = IMU_Dashboard_FloatToScaled(data->accel_x_g, IMU_DASHBOARD_G_STEP,
                                                     -IMU_DASHBOARD_G_STEP, IMU_DASHBOARD_G_STEP);
  Point2D_t origin = {cx, cy};
  Point2D_t x_axis_end = {(int16_t)(cx + 78 + roll), (int16_t)(cy - pitch)};
  Point2D_t y_axis_end = {(int16_t)(cx - 8), (int16_t)(cy - 68 + pitch)};
  Point2D_t z_axis_end = {(int16_t)(cx - 82 + (roll / 2)),
                          (int16_t)(cy + 50 + (roll / 2) + z_depth)};
  Point2D_t accel_point = {(int16_t)(cx + accel_grid_x), (int16_t)(cy - accel_grid_y)};

  IMU_Dashboard_DrawAxis(origin, x_axis_end, color_x_axis);
  IMU_Dashboard_DrawAxis(origin, y_axis_end, color_y_axis);
  IMU_Dashboard_DrawAxis(origin, z_axis_end, color_z_axis);

  IMU_Dashboard_DrawCircle(accel_point.x, accel_point.y, 5, color_text);
  IMU_Dashboard_DrawLine((Point2D_t){(int16_t)(accel_point.x - 7), accel_point.y},
                         (Point2D_t){(int16_t)(accel_point.x + 7), accel_point.y}, color_text);
  IMU_Dashboard_DrawLine((Point2D_t){accel_point.x, (int16_t)(accel_point.y - 7)},
                         (Point2D_t){accel_point.x, (int16_t)(accel_point.y + 7)}, color_text);

  IMU_Dashboard_FillCircle(cx, cy, 9, color_text);
  IMU_Dashboard_DrawCircle(cx, cy, 12, color_body_edge);
  IMU_Dashboard_DrawText((int16_t)(cx - 18), (int16_t)(cy + 16), "IMU", color_text);

  IMU_Dashboard_DrawText((int16_t)(x_axis_end.x + 8), (int16_t)(x_axis_end.y - 8), "X",
                         color_x_axis);
  IMU_Dashboard_DrawText((int16_t)(y_axis_end.x - 6), (int16_t)(y_axis_end.y - 14), "Y",
                         color_y_axis);
  IMU_Dashboard_DrawText((int16_t)(z_axis_end.x - 14), (int16_t)(z_axis_end.y + 4), "Z",
                         color_z_axis);
}

static void IMU_Dashboard_DrawAxis(Point2D_t start, Point2D_t end, uint16_t color) {
  IMU_Dashboard_DrawThickLine(start, end, color);
  IMU_Dashboard_DrawArrowHead(start, end, color);
}

static void IMU_Dashboard_DrawArrowHead(Point2D_t start, Point2D_t end, uint16_t color) {
  int16_t dx = (int16_t)(end.x - start.x);
  int16_t dy = (int16_t)(end.y - start.y);
  int16_t abs_dx = (dx < 0) ? (int16_t)-dx : dx;
  int16_t abs_dy = (dy < 0) ? (int16_t)-dy : dy;
  int16_t dominant = (abs_dx > abs_dy) ? abs_dx : abs_dy;
  int16_t head_length = 13;
  int16_t head_width = 7;
  Point2D_t base;
  Point2D_t side_a;
  Point2D_t side_b;

  if (dominant == 0) {
    return;
  }

  base.x = (int16_t)(end.x - ((dx * head_length) / dominant));
  base.y = (int16_t)(end.y - ((dy * head_length) / dominant));
  side_a.x = (int16_t)(base.x + ((dy * head_width) / dominant));
  side_a.y = (int16_t)(base.y - ((dx * head_width) / dominant));
  side_b.x = (int16_t)(base.x - ((dy * head_width) / dominant));
  side_b.y = (int16_t)(base.y + ((dx * head_width) / dominant));

  IMU_Dashboard_DrawThickLine(end, side_a, color);
  IMU_Dashboard_DrawThickLine(end, side_b, color);
  IMU_Dashboard_DrawLine(side_a, side_b, color);
}

static void IMU_Dashboard_DrawReadouts(const MPU6050_Data_t *data) {
  char text[8];

  IMU_Dashboard_FormatCenti(text, "AX", data->accel_x_g);
  IMU_Dashboard_DrawText(10, 188, text, color_x_axis);
  IMU_Dashboard_FormatCenti(text, "AY", data->accel_y_g);
  IMU_Dashboard_DrawText(84, 188, text, color_y_axis);
  IMU_Dashboard_FormatCenti(text, "AZ", data->accel_z_g);
  IMU_Dashboard_DrawText(158, 188, text, color_z_axis);

  IMU_Dashboard_FormatInteger(text, "GX", data->gyro_x_dps);
  IMU_Dashboard_DrawText(10, 214, text, color_x_axis);
  IMU_Dashboard_FormatInteger(text, "GY", data->gyro_y_dps);
  IMU_Dashboard_DrawText(84, 214, text, color_y_axis);
  IMU_Dashboard_FormatInteger(text, "GZ", data->gyro_z_dps);
  IMU_Dashboard_DrawText(158, 214, text, color_z_axis);
}

static void IMU_Dashboard_DrawText(int16_t x, int16_t y, const char *text, uint16_t color) {
  while (*text != '\0') {
    IMU_Dashboard_DrawChar(x, y, *text, color);
    x = (int16_t)(x + 6);
    text++;
  }
}

static void IMU_Dashboard_DrawChar(int16_t x, int16_t y, char value, uint16_t color) {
  const uint8_t *glyph = IMU_Dashboard_Glyph(value);

  for (uint8_t column = 0U; column < 5U; column++) {
    uint8_t bits = glyph[column];

    for (uint8_t row = 0U; row < 7U; row++) {
      if ((bits & (1U << row)) != 0U) {
        IMU_Dashboard_DrawPixel((int16_t)(x + column), (int16_t)(y + row), color);
      }
    }
  }
}

static const uint8_t *IMU_Dashboard_Glyph(char value) {
  static const uint8_t space[5] = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
  static const uint8_t plus[5] = {0x08U, 0x08U, 0x3EU, 0x08U, 0x08U};
  static const uint8_t minus[5] = {0x08U, 0x08U, 0x08U, 0x08U, 0x08U};
  static const uint8_t dot[5] = {0x00U, 0x60U, 0x60U, 0x00U, 0x00U};
  static const uint8_t digits[10][5] = {
      {0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU}, {0x00U, 0x42U, 0x7FU, 0x40U, 0x00U},
      {0x42U, 0x61U, 0x51U, 0x49U, 0x46U}, {0x21U, 0x41U, 0x45U, 0x4BU, 0x31U},
      {0x18U, 0x14U, 0x12U, 0x7FU, 0x10U}, {0x27U, 0x45U, 0x45U, 0x45U, 0x39U},
      {0x3CU, 0x4AU, 0x49U, 0x49U, 0x30U}, {0x01U, 0x71U, 0x09U, 0x05U, 0x03U},
      {0x36U, 0x49U, 0x49U, 0x49U, 0x36U}, {0x06U, 0x49U, 0x49U, 0x29U, 0x1EU},
  };
  static const uint8_t letter_a[5] = {0x7EU, 0x11U, 0x11U, 0x11U, 0x7EU};
  static const uint8_t letter_e[5] = {0x7FU, 0x49U, 0x49U, 0x49U, 0x41U};
  static const uint8_t letter_g[5] = {0x3EU, 0x41U, 0x49U, 0x49U, 0x7AU};
  static const uint8_t letter_i[5] = {0x00U, 0x41U, 0x7FU, 0x41U, 0x00U};
  static const uint8_t letter_l[5] = {0x7FU, 0x40U, 0x40U, 0x40U, 0x40U};
  static const uint8_t letter_m[5] = {0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU};
  static const uint8_t letter_u[5] = {0x3FU, 0x40U, 0x40U, 0x40U, 0x3FU};
  static const uint8_t letter_v[5] = {0x1FU, 0x20U, 0x40U, 0x20U, 0x1FU};
  static const uint8_t letter_x[5] = {0x63U, 0x14U, 0x08U, 0x14U, 0x63U};
  static const uint8_t letter_y[5] = {0x07U, 0x08U, 0x70U, 0x08U, 0x07U};
  static const uint8_t letter_z[5] = {0x61U, 0x51U, 0x49U, 0x45U, 0x43U};

  if ((value >= '0') && (value <= '9')) {
    return digits[value - '0'];
  }

  switch (value) {
  case '+':
    return plus;
  case '-':
    return minus;
  case '.':
    return dot;
  case 'A':
    return letter_a;
  case 'E':
    return letter_e;
  case 'G':
    return letter_g;
  case 'I':
    return letter_i;
  case 'L':
    return letter_l;
  case 'M':
    return letter_m;
  case 'U':
    return letter_u;
  case 'V':
    return letter_v;
  case 'X':
    return letter_x;
  case 'Y':
    return letter_y;
  case 'Z':
    return letter_z;
  default:
    return space;
  }
}

static int16_t IMU_Dashboard_ClampInt16(int16_t value, int16_t min, int16_t max) {
  if (value < min) {
    return min;
  }

  if (value > max) {
    return max;
  }

  return value;
}

static int16_t IMU_Dashboard_FloatToScaled(float value, float scale, int16_t min, int16_t max) {
  int16_t scaled = (int16_t)(value * scale);

  return IMU_Dashboard_ClampInt16(scaled, min, max);
}

static void IMU_Dashboard_FormatCenti(char *buffer, const char *label, float value) {
  int16_t centi = IMU_Dashboard_FloatToScaled(value, 100.0f, -999, 999);
  uint16_t magnitude = (centi < 0) ? (uint16_t)-centi : (uint16_t)centi;

  buffer[0] = label[0];
  buffer[1] = label[1];
  buffer[2] = (centi < 0) ? '-' : '+';
  buffer[3] = (char)('0' + (magnitude / 100U));
  buffer[4] = '.';
  buffer[5] = (char)('0' + ((magnitude / 10U) % 10U));
  buffer[6] = (char)('0' + (magnitude % 10U));
  buffer[7] = '\0';
}

static void IMU_Dashboard_FormatInteger(char *buffer, const char *label, float value) {
  int16_t integer = IMU_Dashboard_FloatToScaled(value, 1.0f, -999, 999);
  uint16_t magnitude = (integer < 0) ? (uint16_t)-integer : (uint16_t)integer;

  buffer[0] = label[0];
  buffer[1] = label[1];
  buffer[2] = (integer < 0) ? '-' : '+';
  buffer[3] = (char)('0' + ((magnitude / 100U) % 10U));
  buffer[4] = (char)('0' + ((magnitude / 10U) % 10U));
  buffer[5] = (char)('0' + (magnitude % 10U));
  buffer[6] = '\0';
}
