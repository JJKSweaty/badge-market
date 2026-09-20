#include "storage.hpp"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
namespace storage {
static nvs_handle_t handle;
static bool ready;
static uint8_t buffer[bm::MaxSave], verify[bm::MaxSave];
bool init() {
  auto e = nvs_flash_init_partition("bm_store");
  // This partition belongs exclusively to the native game. On first native
  // boot it can still contain bytes from the former stock filesystem.
  if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW("save", "initializing dedicated bm_store partition");
    e = nvs_flash_erase_partition("bm_store");
    if (e == ESP_OK)
      e = nvs_flash_init_partition("bm_store");
  }
  if (e != ESP_OK) {
    ESP_LOGE("save", "dedicated save partition: %s", esp_err_to_name(e));
    return false;
  }
  ready = nvs_open_from_partition("bm_store", "market", NVS_READWRITE,
                                  &handle) == ESP_OK;
  return ready;
}
static bool slot(const char *name, bm::Market &out) {
  size_t n = sizeof buffer;
  return nvs_get_blob(handle, name, buffer, &n) == ESP_OK &&
         bm::decode(out, buffer, n, true);
}
bool load(bool host, bm::Market &out) {
  if (!ready)
    return false;
  bm::Market a, b;
  bool aa = slot(host ? "host_a" : "solo_a", a),
       bb = slot(host ? "host_b" : "solo_b", b);
  if (!aa && !bb)
    return false;
  out = aa && (!bb || a.revision >= b.revision) ? a : b;
  return true;
}
bool save(bool host, bm::Market &m) {
  if (!ready)
    return false;
  if (!m.dirty)
    return true;
  size_t n = bm::encode(m, buffer, sizeof buffer);
  if (!n)
    return false;
  // Write the older/invalid slot, preserving the latest verified checkpoint.
  bm::Market a, b;
  bool aa = slot(host ? "host_a" : "solo_a", a),
       bb = slot(host ? "host_b" : "solo_b", b);
  const char *key = (!aa || (bb && a.revision <= b.revision))
                        ? (host ? "host_a" : "solo_a")
                        : (host ? "host_b" : "solo_b");
  n = bm::encode(m, buffer, sizeof buffer);
  if (nvs_set_blob(handle, key, buffer, n) != ESP_OK ||
      nvs_commit(handle) != ESP_OK)
    return false;
  size_t size = sizeof verify;
  if (nvs_get_blob(handle, key, verify, &size) != ESP_OK || size != n ||
      std::memcmp(buffer, verify, n))
    return false;
  m.dirty = false;
  return true;
}
} // namespace storage
