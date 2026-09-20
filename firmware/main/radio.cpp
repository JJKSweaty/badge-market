#include "radio.hpp"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include <atomic>
namespace radio {
static QueueHandle_t queue;
static std::atomic<bool> busy{false};
static std::atomic<uint32_t> drops{0};
static bool nvsReady, wifiReady, wifiStarted, nowReady, enabled;
static void received(const esp_now_recv_info_t *info, const uint8_t *data,
                     int len) {
  if (!info || len < 24 || len > int(bm::PacketMax))
    return;
  Packet p{};
  std::memcpy(p.from.b, info->src_addr, 6);
  p.rssi = info->rx_ctrl->rssi;
  p.size = len;
  std::memcpy(p.data, data, len);
  if (xQueueSend(queue, &p, 0) != pdTRUE)
    drops++;
}
static void sent(const esp_now_send_info_t *, esp_now_send_status_t) {
  busy = false;
}
esp_err_t init(bm::Mac &mac) {
  queue = xQueueCreate(16, sizeof(Packet));
  if (!queue)
    return ESP_ERR_NO_MEM;
  // Identity does not require powering the RF circuitry. Boot/Solo stay
  // offline.
  return esp_read_mac(mac.b, ESP_MAC_WIFI_STA);
}
esp_err_t disable() {
  enabled = false;
  if (nowReady) {
    auto err = esp_now_deinit();
    if (err != ESP_OK)
      return err;
    nowReady = false;
  }
  if (wifiStarted) {
    auto err = esp_wifi_stop();
    if (err != ESP_OK)
      return err;
    wifiStarted = false;
  }
  busy = false;
  if (queue)
    xQueueReset(queue);
  return ESP_OK;
}
esp_err_t enable() {
  if (enabled)
    return ESP_OK;
  // A partial initialization can be retried from the menu without a reboot.
  auto err = disable();
  if (err != ESP_OK)
    return err;
  auto setup = []() -> esp_err_t {
    esp_err_t e;
    if (!nvsReady) {
      // bm_store is the game save partition, not the default NVS partition
      // used by the PHY. Without this init the SDK repeats full RF calibration
      // on every cold start and cannot retain its calibration results.
      // Preserve existing NVS contents; a failure is a retryable menu error.
      if ((e = nvs_flash_init()) != ESP_OK)
        return e;
      nvsReady = true;
    }
    if (!wifiReady) {
      if ((e = esp_netif_init()) != ESP_OK)
        return e;
      e = esp_event_loop_create_default();
      if (e != ESP_OK && e != ESP_ERR_INVALID_STATE)
        return e;
      wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
      if ((e = esp_wifi_init(&cfg)) != ESP_OK)
        return e;
      wifiReady = true;
    }
    if ((e = esp_wifi_set_storage(WIFI_STORAGE_RAM)) != ESP_OK ||
        (e = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK ||
        (e = esp_wifi_start()) != ESP_OK)
      return e;
    wifiStarted = true;
    // PHY startup is capped separately at 10 dBm in sdkconfig.defaults.
    // 8 quarter-dBm = 2 dBm, the SDK's minimum runtime setting. The PHY
    // startup cap is distinct and still applies before this API can run.
    // Keep continuous reception: unsynchronized ESP-NOW sleeping loses peers.
    if ((e = esp_wifi_set_max_tx_power(8)) != ESP_OK ||
        (e = esp_wifi_set_ps(WIFI_PS_NONE)) != ESP_OK ||
        (e = esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE)) != ESP_OK ||
        (e = esp_now_init()) != ESP_OK)
      return e;
    nowReady = true;
    if ((e = esp_now_register_recv_cb(received)) != ESP_OK ||
        (e = esp_now_register_send_cb(sent)) != ESP_OK)
      return e;
    esp_now_peer_info_t peer{};
    std::memset(peer.peer_addr, 255, 6);
    peer.channel = 6;
    peer.ifidx = WIFI_IF_STA;
    return esp_now_add_peer(&peer);
  };
  err = setup();
  if (err != ESP_OK) {
    ESP_LOGE("radio", "startup failed: %s", esp_err_to_name(err));
    disable();
    return err;
  }
  enabled = true;
  ESP_LOGI("radio", "ready: channel=6 tx_power_qdbm=%d", tx_power());
  return ESP_OK;
}
bool active() { return enabled; }
int tx_power() {
  int8_t power;
  return enabled && esp_wifi_get_max_tx_power(&power) == ESP_OK ? power : -1;
}
bool send(void *, const uint8_t *b, size_t n) {
  if (!enabled || n > bm::PacketMax || busy.exchange(true))
    return false;
  static constexpr uint8_t target[] = {255, 255, 255, 255, 255, 255};
  if (esp_now_send(target, b, n) != ESP_OK) {
    busy = false;
    return false;
  }
  return true;
}
bool receive(Packet &p) {
  return enabled && queue && xQueueReceive(queue, &p, 0) == pdTRUE;
}
uint32_t dropped() { return drops.load(); }
} // namespace radio
