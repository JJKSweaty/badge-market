#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
using esp_err_t = int;
constexpr int ESP_OK = 0;
#define ESP_ERROR_CHECK(x) assert((x) == 0)
#define ESP_LOGI(...) ((void)0)
#define pdMS_TO_TICKS(x) (x)
inline uint32_t esp_get_free_heap_size() { return 180000; }
inline uint32_t esp_get_minimum_free_heap_size() { return 170000; }
inline unsigned uxTaskGetStackHighWaterMark(void *) { return 8000; }
inline uint32_t esp_random() { return 1234567; }
inline void vTaskDelay(unsigned) {}
struct usb_serial_jtag_driver_config_t {
  int rx_buffer_size, tx_buffer_size;
};
inline int
usb_serial_jtag_driver_install(const usb_serial_jtag_driver_config_t *) {
  return 0;
}
inline int usb_serial_jtag_read_bytes(void *, size_t, int) { return 0; }

enum esp_reset_reason_t {
  ESP_RST_UNKNOWN,
  ESP_RST_PANIC,
  ESP_RST_INT_WDT,
  ESP_RST_TASK_WDT,
  ESP_RST_WDT,
  ESP_RST_BROWNOUT
};
inline esp_reset_reason_t esp_reset_reason() { return ESP_RST_UNKNOWN; }
