#pragma once
#include "esp_err.h"
#include "session.hpp"
namespace radio {
struct Packet {
  bm::Mac from;
  int8_t rssi;
  uint16_t size;
  uint8_t data[bm::PacketMax];
};
esp_err_t init(bm::Mac &);
esp_err_t enable();
esp_err_t disable();
bool active();
int tx_power(); // quarter-dBm, or -1 while off/unavailable
bool send(void *, const uint8_t *, size_t);
bool receive(Packet &);
uint32_t dropped();
} // namespace radio
