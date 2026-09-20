#include "board.hpp"
#include "display.hpp"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <algorithm>
#include <cctype>
#include <cstring>
namespace board {
static esp_lcd_panel_handle_t panel;
static SemaphoreHandle_t lcdDone;
static uint16_t *stripe;
static constexpr int StripeH = 16;
static i2c_master_dev_handle_t accelerometer;
static bool accelOk;
static rmt_channel_handle_t ledChannel;
static rmt_encoder_handle_t ledEncoder;
static rmt_symbol_word_t symbols[145];
uint64_t ms() { return esp_timer_get_time() / 1000; }
static bool color_done(esp_lcd_panel_io_handle_t,
                       esp_lcd_panel_io_event_data_t *, void *) {
  BaseType_t wake = pdFALSE;
  xSemaphoreGiveFromISR(lcdDone, &wake);
  return wake == pdTRUE;
}
static esp_err_t reg_write(uint8_t reg, uint8_t val) {
  uint8_t data[] = {reg, val};
  return i2c_master_transmit(accelerometer, data, 2, 15);
}
esp_err_t init() {
  gpio_config_t out{};
  out.pin_bit_mask = (1ULL << 20) | (1ULL << 21);
  out.mode = GPIO_MODE_OUTPUT;
  ESP_ERROR_CHECK(gpio_config(&out));
  gpio_set_level(GPIO_NUM_20, 1);
  gpio_set_level(GPIO_NUM_21, 0);
  gpio_config_t in{};
  in.pin_bit_mask = (1ULL << 7) | (1ULL << 9);
  in.mode = GPIO_MODE_INPUT;
  in.pull_up_en = GPIO_PULLUP_ENABLE;
  ESP_ERROR_CHECK(gpio_config(&in));
  lcdDone = xSemaphoreCreateBinary();
  if (!lcdDone)
    return ESP_ERR_NO_MEM;
  stripe = static_cast<uint16_t *>(heap_caps_malloc(
      320 * StripeH * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  if (!stripe)
    return ESP_ERR_NO_MEM;
  spi_bus_config_t spi{};
  spi.mosi_io_num = 10;
  spi.miso_io_num = -1;
  spi.sclk_io_num = 1;
  spi.quadwp_io_num = -1;
  spi.quadhd_io_num = -1;
  spi.max_transfer_sz = 320 * StripeH * 2;
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &spi, SPI_DMA_CH_AUTO));
  esp_lcd_panel_io_spi_config_t io{};
  io.cs_gpio_num = 2;
  io.dc_gpio_num = 0;
  io.spi_mode = 0;
  io.pclk_hz = 40000000;
  io.trans_queue_depth = 1;
  io.lcd_cmd_bits = 8;
  io.lcd_param_bits = 8;
  io.on_color_trans_done = color_done;
  esp_lcd_panel_io_handle_t handle;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io, &handle));
  esp_lcd_panel_dev_config_t cfg{};
  cfg.reset_gpio_num = 4;
  cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
  cfg.bits_per_pixel = 16;
  ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(handle, &cfg, &panel));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
  ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, true));
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, true, false));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
  i2c_master_bus_config_t bus{};
  bus.i2c_port = I2C_NUM_0;
  bus.sda_io_num = GPIO_NUM_5;
  bus.scl_io_num = GPIO_NUM_6;
  bus.clk_source = I2C_CLK_SRC_DEFAULT;
  bus.glitch_ignore_cnt = 7;
  bus.flags.enable_internal_pullup = true;
  i2c_master_bus_handle_t bh;
  auto err = i2c_new_master_bus(&bus, &bh);
  if (err == ESP_OK) {
    i2c_device_config_t dev{};
    dev.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev.device_address = 0x19;
    dev.scl_speed_hz = 400000;
    dev.scl_wait_us = 10000;
    err = i2c_master_bus_add_device(bh, &dev, &accelerometer);
    if (err == ESP_OK) {
      uint8_t reg = 0x0f, id = 0;
      err = i2c_master_transmit_receive(accelerometer, &reg, 1, &id, 1, 15);
      accelOk = err == ESP_OK && id == 0x11 &&
                reg_write(0x20, 0x57) == ESP_OK &&
                reg_write(0x23, 0x80) == ESP_OK;
    }
  }
  ESP_LOGI("board", "accelerometer: %s",
           accelOk ? "SC7A20 ready" : "unavailable; D-pad fallback");
  rmt_tx_channel_config_t tx{};
  tx.gpio_num = GPIO_NUM_3;
  tx.clk_src = RMT_CLK_SRC_DEFAULT;
  tx.resolution_hz = 10000000;
  tx.mem_block_symbols = 64;
  tx.trans_queue_depth = 1;
  ESP_ERROR_CHECK(rmt_new_tx_channel(&tx, &ledChannel));
  rmt_copy_encoder_config_t enc{};
  ESP_ERROR_CHECK(rmt_new_copy_encoder(&enc, &ledEncoder));
  ESP_ERROR_CHECK(rmt_enable(ledChannel));
  leds(0, 0, 0);
  return ESP_OK;
}
uint16_t buttons(uint64_t now) {
  gpio_set_level(GPIO_NUM_20, 0);
  esp_rom_delay_us(1);
  gpio_set_level(GPIO_NUM_20, 1);
  esp_rom_delay_us(1);
  uint16_t raw = 0;
  for (int i = 0; i < 8; i++) {
    if (!gpio_get_level(GPIO_NUM_7))
      raw |= 1 << i;
    gpio_set_level(GPIO_NUM_21, 1);
    esp_rom_delay_us(1);
    gpio_set_level(GPIO_NUM_21, 0);
    esp_rom_delay_us(1);
  }
  if (!gpio_get_level(GPIO_NUM_9))
    raw |= 1 << Start;
  static uint16_t candidate = 0, stable = 0;
  static uint64_t since[9]{};
  for (int i = 0; i < 9; i++) {
    uint16_t bit = 1 << i;
    if ((raw & bit) != (candidate & bit)) {
      candidate ^= bit;
      since[i] = now;
    }
    if (now - since[i] >= 25)
      stable = (stable & ~bit) | (candidate & bit);
  }
  return stable;
}
bool accel(int &x, int &y) {
  if (!accelOk)
    return false;
  uint8_t reg = 0x27, status = 0;
  if (i2c_master_transmit_receive(accelerometer, &reg, 1, &status, 1, 15) !=
      ESP_OK)
    return false;
  if (!(status & 8))
    return false;
  reg = 0xa8;
  uint8_t data[6];
  if (i2c_master_transmit_receive(accelerometer, &reg, 1, data, 6, 15) !=
      ESP_OK)
    return false;
  x = int16_t(data[0] | (data[1] << 8)) / 16;
  y = int16_t(data[2] | (data[3] << 8)) / 16;
  return true;
}
void leds(uint8_t red, uint8_t green, uint8_t blue, int active) {
  if (!ledChannel)
    return;
  // The encoder reads symbols asynchronously. Never overwrite an in-flight
  // frame, and never reboot the game because an optional LED frame is late.
  static bool reported = false;
  auto previous = rmt_tx_wait_all_done(ledChannel, 0);
  if (previous != ESP_OK) {
    if (!reported)
      ESP_LOGW("board", "LED frame deferred: %s", esp_err_to_name(previous));
    reported = true;
    return;
  }
  unsigned k = 0;
  for (int i = 0; i < 6; i++) {
    uint8_t c[] = {
        uint8_t(active < 0 || active == i ? std::min(24, int(green)) : 0),
        uint8_t(active < 0 || active == i ? std::min(24, int(red)) : 0),
        uint8_t(active < 0 || active == i ? std::min(24, int(blue)) : 0)};
    for (auto v : c)
      for (int bit = 7; bit >= 0; bit--) {
        auto &s = symbols[k++];
        s.level0 = 1;
        s.duration0 = (v & (1 << bit)) ? 8 : 3;
        s.level1 = 0;
        s.duration1 = (v & (1 << bit)) ? 4 : 9;
      }
  }
  symbols[k].level0 = 0;
  symbols[k].duration0 = 400;
  symbols[k].level1 = 0;
  symbols[k].duration1 = 400;
  rmt_transmit_config_t cfg{};
  auto result =
      rmt_transmit(ledChannel, ledEncoder, symbols, sizeof symbols, &cfg);
  if (result != ESP_OK && !reported)
    ESP_LOGW("board", "LED frame skipped: %s", esp_err_to_name(result));
  reported = result != ESP_OK;
}
void render(const Screen &f) {
  display::Painter painter{stripe};
  for (int sy = 0; sy < 240; sy += StripeH) {
    painter.paint(f, sy);
    ESP_ERROR_CHECK(
        esp_lcd_panel_draw_bitmap(panel, 0, sy, 320, sy + StripeH, stripe));
    if (xSemaphoreTake(lcdDone, pdMS_TO_TICKS(100)) != pdTRUE)
      ESP_ERROR_CHECK(ESP_ERR_TIMEOUT);
  }
}
} // namespace board
