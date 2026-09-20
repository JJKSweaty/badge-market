#pragma once
#include "esp_err.h"
#include <cstdint>
namespace board {
enum Button { A = 0, B, Home, Down, Left, Right, Up, Aux, Start };
constexpr uint16_t rgb(unsigned r, unsigned g, unsigned b) {
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}
constexpr uint16_t Paper = rgb(244, 240, 229), Ink = rgb(28, 37, 32),
                   Green = rgb(57, 84, 64), Blue = rgb(47, 81, 111),
                   Red = rgb(155, 54, 40), Muted = rgb(83, 94, 83),
                   Rule = rgb(198, 199, 183), Gold = rgb(211, 162, 75);
struct Screen {
  char title[24]{}, subtitle[52]{}, rows[6][32]{}, footer[52]{}, status[52]{};
  int selected = -1;
  uint16_t accent = Green;
};
esp_err_t init();
uint16_t buttons(uint64_t now);
bool accel(int &x, int &y);
void render(const Screen &);
void leds(uint8_t red, uint8_t green, uint8_t blue, int active = -1);
uint64_t ms();
} // namespace board
