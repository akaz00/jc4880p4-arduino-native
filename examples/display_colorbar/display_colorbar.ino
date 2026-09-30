/*
 * display_colorbar.ino
 * Minimal test: bring up the panel and draw 7 vertical color bars. No LVGL, no touch.
 *
 * Board  : Guition JC4880P443C_I_W (ESP32-P4 + ESP32-C6, 4.3" 480x800 ST7701S MIPI-DSI)
 * Setup  : see the top-level README.md (Arduino IDE board settings + requirements)
 *
 * Display bring-up values (LDO / DSI / DPI timing / ST7701 init table) come from
 * ultramcu/guition-jc4880p4-bsp (MIT) -- https://github.com/ultramcu/guition-jc4880p4-bsp
 * This sketch re-implements that recipe with plain ESP-IDF calls inside Arduino IDE.
 */

#include <Arduino.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_cache.h"
#include "driver/gpio.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

static const char *TAG = "jc4880p4";

// --- 검증된 핀/파라미터 ---
#define LCD_H_RES        480
#define LCD_V_RES        800
#define LCD_RST_PIN      5
#define LCD_BL_PIN       23

#define DSI_LANE_NUM     2
#define DSI_LANE_MBPS    500
#define DPI_CLK_MHZ      34
#define DSI_LDO_CHAN     3
#define DSI_LDO_MV       2500

// --- ST7701 초기화 커맨드 테이블 (board_p4_st7701_init.h 그대로) ---
typedef struct {
  uint8_t cmd;
  const uint8_t *data;
  size_t len;
  uint16_t delay_ms;
} panel_init_cmd_t;

static const uint8_t c_ff_13[] = {0x77, 0x01, 0x00, 0x00, 0x13};
static const uint8_t c_ef_08[] = {0x08};
static const uint8_t c_ff_10[] = {0x77, 0x01, 0x00, 0x00, 0x10};
static const uint8_t c_c0[]    = {0x63, 0x00};
static const uint8_t c_c1[]    = {0x0D, 0x02};
static const uint8_t c_c2[]    = {0x10, 0x08};
static const uint8_t c_cc[]    = {0x10};
static const uint8_t c_b0g[]   = {0x80,0x09,0x53,0x0C,0xD0,0x07,0x0C,0x09,0x09,0x28,0x06,0xD4,0x13,0x69,0x2B,0x71};
static const uint8_t c_b1g[]   = {0x80,0x94,0x5A,0x10,0xD3,0x06,0x0A,0x08,0x08,0x25,0x03,0xD3,0x12,0x66,0x6A,0x0D};
static const uint8_t c_ff_11[] = {0x77, 0x01, 0x00, 0x00, 0x11};
static const uint8_t c_b0[]    = {0x5D};
static const uint8_t c_b1[]    = {0x58};
static const uint8_t c_b2[]    = {0x87};
static const uint8_t c_b3[]    = {0x80};
static const uint8_t c_b5[]    = {0x4E};
static const uint8_t c_b7[]    = {0x85};
static const uint8_t c_b8[]    = {0x21};
static const uint8_t c_b9[]    = {0x10, 0x1F};
static const uint8_t c_bb[]    = {0x03};
static const uint8_t c_bc[]    = {0x00};
static const uint8_t c_c1b[]   = {0x78};
static const uint8_t c_c2b[]   = {0x78};
static const uint8_t c_d0[]    = {0x88};
static const uint8_t c_e0[]    = {0x00, 0x3A, 0x02};
static const uint8_t c_e1[]    = {0x04,0xA0,0x00,0xA0,0x05,0xA0,0x00,0xA0,0x00,0x40,0x40};
static const uint8_t c_e2[]    = {0x30,0x00,0x40,0x40,0x32,0xA0,0x00,0xA0,0x00,0xA0,0x00,0xA0,0x00};
static const uint8_t c_e3[]    = {0x00, 0x00, 0x33, 0x33};
static const uint8_t c_e4[]    = {0x44, 0x44};
static const uint8_t c_e5[]    = {0x09,0x2E,0xA0,0xA0,0x0B,0x30,0xA0,0xA0,0x05,0x2A,0xA0,0xA0,0x07,0x2C,0xA0,0xA0};
static const uint8_t c_e6[]    = {0x00, 0x00, 0x33, 0x33};
static const uint8_t c_e7[]    = {0x44, 0x44};
static const uint8_t c_e8[]    = {0x08,0x2D,0xA0,0xA0,0x0A,0x2F,0xA0,0xA0,0x04,0x29,0xA0,0xA0,0x06,0x2B,0xA0,0xA0};
static const uint8_t c_eb[]    = {0x00, 0x00, 0x4E, 0x4E, 0x00, 0x00, 0x00};
static const uint8_t c_ec[]    = {0x08, 0x01};
static const uint8_t c_ed[]    = {0xB0,0x2B,0x98,0xA4,0x56,0x7F,0xFF,0xFF,0xFF,0xFF,0xF7,0x65,0x4A,0x89,0xB2,0x0B};
static const uint8_t c_ef[]    = {0x08, 0x08, 0x08, 0x45, 0x3F, 0x54};
static const uint8_t c_ff_00[] = {0x77, 0x01, 0x00, 0x00, 0x00};
static const uint8_t c_nop[]   = {0x00};

static const panel_init_cmd_t init_cmds[] = {
  {0xFF, c_ff_13, sizeof(c_ff_13), 0},
  {0xEF, c_ef_08, sizeof(c_ef_08), 0},
  {0xFF, c_ff_10, sizeof(c_ff_10), 0},
  {0xC0, c_c0,    sizeof(c_c0),    0},
  {0xC1, c_c1,    sizeof(c_c1),    0},
  {0xC2, c_c2,    sizeof(c_c2),    0},
  {0xCC, c_cc,    sizeof(c_cc),    0},
  {0xB0, c_b0g,   sizeof(c_b0g),   0},
  {0xB1, c_b1g,   sizeof(c_b1g),   0},
  {0xFF, c_ff_11, sizeof(c_ff_11), 0},
  {0xB0, c_b0,    sizeof(c_b0),    0},
  {0xB1, c_b1,    sizeof(c_b1),    0},
  {0xB2, c_b2,    sizeof(c_b2),    0},
  {0xB3, c_b3,    sizeof(c_b3),    0},
  {0xB5, c_b5,    sizeof(c_b5),    0},
  {0xB7, c_b7,    sizeof(c_b7),    0},
  {0xB8, c_b8,    sizeof(c_b8),    0},
  {0xB9, c_b9,    sizeof(c_b9),    0},
  {0xBB, c_bb,    sizeof(c_bb),    0},
  {0xBC, c_bc,    sizeof(c_bc),    0},
  {0xC1, c_c1b,   sizeof(c_c1b),   0},
  {0xC2, c_c2b,   sizeof(c_c2b),   0},
  {0xD0, c_d0,    sizeof(c_d0),    0},
  {0xE0, c_e0,    sizeof(c_e0),    0},
  {0xE1, c_e1,    sizeof(c_e1),    0},
  {0xE2, c_e2,    sizeof(c_e2),    0},
  {0xE3, c_e3,    sizeof(c_e3),    0},
  {0xE4, c_e4,    sizeof(c_e4),    0},
  {0xE5, c_e5,    sizeof(c_e5),    0},
  {0xE6, c_e6,    sizeof(c_e6),    0},
  {0xE7, c_e7,    sizeof(c_e7),    0},
  {0xE8, c_e8,    sizeof(c_e8),    0},
  {0xEB, c_eb,    sizeof(c_eb),    0},
  {0xEC, c_ec,    sizeof(c_ec),    0},
  {0xED, c_ed,    sizeof(c_ed),    0},
  {0xEF, c_ef,    sizeof(c_ef),    0},
  {0xFF, c_ff_00, sizeof(c_ff_00), 0},
  {0x11, c_nop,   sizeof(c_nop),   120}, // SLPOUT
  {0x29, c_nop,   sizeof(c_nop),   20},  // DISPON
};
#define INIT_CMDS_SIZE (sizeof(init_cmds) / sizeof(init_cmds[0]))

static esp_lcd_dsi_bus_handle_t  s_dsi_bus = NULL;
static esp_lcd_panel_io_handle_t s_dbi_io  = NULL;
static esp_lcd_panel_handle_t    s_panel   = NULL;

static void panel_reset(void) {
  gpio_config_t rst = {
    .pin_bit_mask = 1ULL << LCD_RST_PIN,
    .mode = GPIO_MODE_OUTPUT,
  };
  gpio_config(&rst);
  gpio_set_level((gpio_num_t)LCD_RST_PIN, 0);
  delay(20);
  gpio_set_level((gpio_num_t)LCD_RST_PIN, 1);
  delay(120);
  ESP_LOGI(TAG, "panel reset pulsed on GPIO%d", LCD_RST_PIN);
}

static esp_err_t send_init_seq(void) {
  for (size_t i = 0; i < INIT_CMDS_SIZE; i++) {
    const panel_init_cmd_t *e = &init_cmds[i];
    esp_err_t err = esp_lcd_panel_io_tx_param(s_dbi_io, e->cmd, e->data, e->len);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "tx_param cmd 0x%02X failed: %s", e->cmd, esp_err_to_name(err));
      return err;
    }
    if (e->delay_ms) delay(e->delay_ms);
  }
  return ESP_OK;
}

static esp_err_t display_init(void) {
  ESP_LOGI(TAG, "=== display init (raw ESP-IDF, no esp_lcd_st7701) ===");

  // 1. DSI-PHY 전원 (LDO chan 3, 2500mV)
  esp_ldo_channel_handle_t ldo_phy = NULL;
  esp_ldo_channel_config_t ldo_cfg = {
    .chan_id = DSI_LDO_CHAN,
    .voltage_mv = DSI_LDO_MV,
  };
  ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo_cfg, &ldo_phy), TAG, "LDO acquire failed");
  ESP_LOGI(TAG, "DSI-PHY LDO chan=%d %dmV OK", DSI_LDO_CHAN, DSI_LDO_MV);

  // 2. DSI 버스 (2-lane, 500Mbps)
  esp_lcd_dsi_bus_config_t bus_cfg = {
    .bus_id = 0,
    .num_data_lanes = DSI_LANE_NUM,
    .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
    .lane_bit_rate_mbps = DSI_LANE_MBPS,
  };
  ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&bus_cfg, &s_dsi_bus), TAG, "dsi bus failed");

  // 3. DBI IO (커맨드 채널)
  esp_lcd_dbi_io_config_t dbi_cfg = {
    .virtual_channel = 0,
    .lcd_cmd_bits = 8,
    .lcd_param_bits = 8,
  };
  ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_dbi(s_dsi_bus, &dbi_cfg, &s_dbi_io), TAG, "dbi io failed");

  // 4. 범용 DPI 패널 생성 (esp_lcd_st7701 컴포넌트 사용 안 함)
  esp_lcd_dpi_panel_config_t dpi_cfg = {
    .virtual_channel = 0,
    .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
    .dpi_clock_freq_mhz = DPI_CLK_MHZ,
    .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
    .num_fbs = 2,
    .video_timing = {
      .h_size = LCD_H_RES,
      .v_size = LCD_V_RES,
      .hsync_pulse_width = 12,
      .hsync_back_porch = 42,
      .hsync_front_porch = 42,
      .vsync_pulse_width = 2,
      .vsync_back_porch = 8,
      .vsync_front_porch = 166,
    },
    .flags = { .use_dma2d = true },
  };
  ESP_RETURN_ON_ERROR(esp_lcd_new_panel_dpi(s_dsi_bus, &dpi_cfg, &s_panel), TAG, "dpi panel failed");

  // 5. 패널 물리 리셋 (GPIO5 LOW 20ms -> HIGH 120ms)
  panel_reset();

  // 6. ST7701 초기화 커맨드 수동 전송 (esp_lcd_panel_io_tx_param)
  ESP_RETURN_ON_ERROR(send_init_seq(), TAG, "init seq failed");

  // 7. DPI 패널 init (실제 비디오 스트리밍 시작)
  ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "panel init failed");

  ESP_LOGI(TAG, "=== display init done ===");
  return ESP_OK;
}

static void backlight_on(void) {
  ledcAttach(LCD_BL_PIN, 5000, 8);
  ledcWrite(LCD_BL_PIN, 255);
  ESP_LOGI(TAG, "backlight ON (GPIO%d)", LCD_BL_PIN);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== JC4880P443 raw ESP-IDF display test ===");

  esp_err_t err = display_init();
  if (err != ESP_OK) {
    Serial.printf("display_init FAILED: %s\n", esp_err_to_name(err));
    return;
  }

  // 프레임버퍼 얻어서 세로 컬러바 채우기
  void *fb0 = NULL, *fb1 = NULL;
  err = esp_lcd_dpi_panel_get_frame_buffer(s_panel, 2, &fb0, &fb1);
  if (err != ESP_OK) {
    Serial.printf("get_frame_buffer FAILED: %s\n", esp_err_to_name(err));
    return;
  }

  uint16_t *fb = (uint16_t *)fb0;
  const uint16_t colors[] = {0xF800, 0x07E0, 0x001F, 0xFFE0, 0x07FF, 0xF81F, 0xFFFF}; // R G B Y C M W
  int band = LCD_H_RES / 7;
  for (int y = 0; y < LCD_V_RES; y++) {
    for (int x = 0; x < LCD_H_RES; x++) {
      int idx = x / band;
      if (idx > 6) idx = 6;
      fb[y * LCD_H_RES + x] = colors[idx];
    }
  }
  // 캐시->메모리 동기화 (DPI DMA가 최신 데이터를 보도록)
  esp_cache_msync(fb0, (size_t)LCD_H_RES * LCD_V_RES * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

  // 완성된 버퍼를 화면에 present (포인터가 자체 FB라 복사 없이 스왑됨)
  esp_lcd_panel_draw_bitmap(s_panel, 0, 0, LCD_H_RES, LCD_V_RES, fb0);

  backlight_on();

  Serial.println("=== done - 컬러바가 보여야 합니다 ===");
}

void loop() {
  delay(1000);
}
