#pragma once
#include "esp_err.h"
#include <cstdint>
#include <cstdio>
namespace board {
enum Button { A = 0, B, Home, Down, Left, Right, Up, Aux, Start };
constexpr uint16_t rgb(unsigned r, unsigned g, unsigned b) {
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}
constexpr uint16_t Paper = 0x0884, Ink = 0xF7BF, Green = 0x4732, Blue = 0x4D1F,
                   Red = 0xFAED, Muted = 0x8CB5, Rule = 0x2148, Gold = 0xFE4A,
                   Cyan = 0x5F5F, Surface = 0x10C5, Amber = 0xFE8C,
                   Purple = 0xA45F, Disabled = 0x3A0A;
struct Primitive {
  int16_t x{}, y{}, w{}, h{};
  uint16_t color{};
  uint8_t type{}, scale{};
  char text[52]{};
};
struct Screen {
  char title[24]{}, subtitle[52]{}, rows[6][32]{}, footer[52]{}, status[52]{};
  int selected = -1;
  uint16_t accent = Cyan;
  bool custom{};
  int translate{};
  uint16_t background = Paper;
  unsigned count{};
  Primitive commands[128]{};
  void box(int x, int y, int w, int h, uint16_t color, bool outline = false) {
    if (count >= 128)
      return;
    auto &c = commands[count++];
    c.x = x;
    c.y = y;
    c.w = w;
    c.h = h;
    c.color = color;
    c.type = outline ? 1 : 0;
  }
  void text(int x, int y, const char *value, uint16_t color = Ink,
            int scale = 1);
};
inline void Screen::text(int x, int y, const char *value, uint16_t color,
                         int scale) {
  if (count >= 128)
    return;
  auto &c = commands[count++];
  c.x = x;
  c.y = y;
  c.color = color;
  c.scale = scale;
  c.type = 2;
  std::snprintf(c.text, sizeof c.text, "%s", value);
}
esp_err_t init();
uint16_t buttons(uint64_t now);
bool accel(int &x, int &y);
void render(const Screen &);
void leds(uint8_t red, uint8_t green, uint8_t blue, int active = -1);
uint64_t ms();
} // namespace board
