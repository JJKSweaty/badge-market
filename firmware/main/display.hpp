#pragma once
#include "board.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
namespace board::display {
constexpr int StripeH = 8;
// Original compact 5x7 uppercase glyphs, columns stored in flash.
static const uint8_t font[][5] = {{0x7e, 0x11, 0x11, 0x11, 0x7e},
                                  {0x7f, 0x49, 0x49, 0x49, 0x36},
                                  {0x3e, 0x41, 0x41, 0x41, 0x22},
                                  {0x7f, 0x41, 0x41, 0x22, 0x1c},
                                  {0x7f, 0x49, 0x49, 0x49, 0x41},
                                  {0x7f, 0x09, 0x09, 0x09, 0x01},
                                  {0x3e, 0x41, 0x49, 0x49, 0x7a},
                                  {0x7f, 0x08, 0x08, 0x08, 0x7f},
                                  {0, 0x41, 0x7f, 0x41, 0},
                                  {0x20, 0x40, 0x41, 0x3f, 1},
                                  {0x7f, 8, 0x14, 0x22, 0x41},
                                  {0x7f, 0x40, 0x40, 0x40, 0x40},
                                  {0x7f, 2, 0x0c, 2, 0x7f},
                                  {0x7f, 4, 8, 0x10, 0x7f},
                                  {0x3e, 0x41, 0x41, 0x41, 0x3e},
                                  {0x7f, 9, 9, 9, 6},
                                  {0x3e, 0x41, 0x51, 0x21, 0x5e},
                                  {0x7f, 9, 0x19, 0x29, 0x46},
                                  {0x26, 0x49, 0x49, 0x49, 0x32},
                                  {1, 1, 0x7f, 1, 1},
                                  {0x3f, 0x40, 0x40, 0x40, 0x3f},
                                  {0x1f, 0x20, 0x40, 0x20, 0x1f},
                                  {0x7f, 0x20, 0x18, 0x20, 0x7f},
                                  {0x63, 0x14, 8, 0x14, 0x63},
                                  {3, 4, 0x78, 4, 3},
                                  {0x61, 0x51, 0x49, 0x45, 0x43},
                                  {0x3e, 0x51, 0x49, 0x45, 0x3e},
                                  {0, 0x42, 0x7f, 0x40, 0},
                                  {0x62, 0x51, 0x49, 0x49, 0x46},
                                  {0x22, 0x41, 0x49, 0x49, 0x36},
                                  {0x18, 0x14, 0x12, 0x7f, 0x10},
                                  {0x27, 0x45, 0x45, 0x45, 0x39},
                                  {0x3e, 0x49, 0x49, 0x49, 0x32},
                                  {1, 0x71, 9, 5, 3},
                                  {0x36, 0x49, 0x49, 0x49, 0x36},
                                  {6, 0x49, 0x49, 0x49, 0x3e}};
static uint8_t glyph(char ch, int col) {
  ch = std::toupper(static_cast<unsigned char>(ch));
  if (ch >= 'A' && ch <= 'Z')
    return font[ch - 'A'][col];
  if (ch >= '0' && ch <= '9')
    return font[26 + ch - '0'][col];
  switch (ch) {
  case '.':
    return col == 2 ? 0x60 : 0;
  case ':':
    return col == 2 ? 0x24 : 0;
  case '-':
    return 8;
  case '+':
    return col == 2 ? 0x3e : 8;
  case '/':
    return 1 << (6 - col);
  case '>':
    return col == 1 ? 0x22 : col == 2 ? 0x14 : col == 3 ? 8 : 0;
  case '<':
    return col == 3 ? 0x22 : col == 2 ? 0x14 : col == 1 ? 8 : 0;
  case '!':
    return col == 2 ? 0x5f : 0;
  case '?':
    return col == 0 ? 2 : col == 1 ? 1 : col == 2 ? 0x51 : col == 3 ? 9 : 6;
  case '%':
    return col == 0 ? 0x63 : col == 4 ? 0x63 : 1 << (5 - col);
  case '#':
    return col == 1 || col == 3 ? 0x7f : 0x14;
  case '$':
    return col == 2 ? 0x7f : col == 0 ? 0x24 : col == 4 ? 0x12 : 0x2a;
  case '@':
    return col == 0 || col == 4 ? 0x3e : 0x55;
  default:
    return 0;
  }
}
// Cache command fingerprints and vertical bounds, not a second
// scene/framebuffer. Repaint both the old and new extents of every
// changed/removed primitive.
struct Damage {
  uint32_t hashes[128]{};
  uint8_t tops[128]{}, bottoms[128]{};
  unsigned count{};
  uint16_t background{};
  int translate{};
  bool initialized{}, custom{};
  uint32_t update(const Screen &f) {
    uint32_t mask = (!initialized || !f.custom || !custom ||
                     f.background != background || f.translate != translate)
                        ? 0x3fffffffu
                        : 0;
    auto mark = [&](int top, int bottom) {
      top = std::clamp(top, 0, 240);
      bottom = std::clamp(bottom, 0, 240);
      if (bottom > top)
        for (int band = top / StripeH; band <= (bottom - 1) / StripeH; band++)
          mask |= 1u << band;
    };
    for (unsigned i = 0; i < std::max(count, f.count); i++) {
      uint32_t hash = 0;
      int top = 0, bottom = 0;
      if (i < f.count) {
        const auto &c = f.commands[i];
        const auto *bytes = reinterpret_cast<const uint8_t *>(&c);
        hash = 2166136261u;
        for (unsigned j = 0; j < sizeof c; j++)
          hash = (hash ^ bytes[j]) * 16777619u;
        top = std::clamp(int(c.y), 0, 240);
        bottom =
            std::clamp(int(c.y) + (c.type == 2 ? 7 * c.scale : c.h), 0, 240);
      }
      if (hash != hashes[i]) {
        mark(tops[i], bottoms[i]);
        mark(top, bottom);
      }
      hashes[i] = hash;
      tops[i] = top;
      bottoms[i] = bottom;
    }
    count = f.count;
    background = f.background;
    translate = f.translate;
    custom = f.custom;
    initialized = true;
    return mask;
  }
};
struct Painter {
  uint16_t *stripe;
  int shift = 0;
  static uint16_t swap(uint16_t c) { return (c << 8) | (c >> 8); }
  void rect(int sy, int x, int y, int w, int h, uint16_t c) {
    x += shift;
    for (int py = std::max(y, sy); py < std::min(y + h, sy + StripeH); py++)
      for (int px = std::max(0, x); px < std::min(320, x + w); px++)
        stripe[(py - sy) * 320 + px] = swap(c);
  }
  void text(int sy, int x, int y, const char *s, int scale, uint16_t color) {
    for (; *s && x + 5 * scale <= 320; s++, x += 6 * scale) {
      if (y >= sy + StripeH || y + 7 * scale <= sy)
        continue;
      for (int col = 0; col < 5; col++) {
        auto bits = glyph(*s, col);
        for (int row = 0; row < 7; row++)
          if (bits & (1 << row))
            rect(sy, x + col * scale, y + row * scale, scale, scale, color);
      }
    }
  }
  void paint(const Screen &f, int sy) {
    shift = f.translate;
    std::fill_n(stripe, 320 * StripeH, swap(f.background));
    if (f.custom) {
      for (unsigned i = 0; i < f.count; i++) {
        const auto &c = f.commands[i];
        if (c.type == 2)
          text(sy, c.x, c.y, c.text, c.scale, c.color);
        else if (!c.type)
          rect(sy, c.x, c.y, c.w, c.h, c.color);
        else {
          rect(sy, c.x, c.y, c.w, 1, c.color);
          rect(sy, c.x, c.y + c.h - 1, c.w, 1, c.color);
          rect(sy, c.x, c.y, 1, c.h, c.color);
          rect(sy, c.x + c.w - 1, c.y, 1, c.h, c.color);
        }
      }
      return;
    }
    text(sy, 14, 12, f.title, 2, Ink);
    text(sy, 14, 40, f.subtitle, 1,
         std::strstr(f.subtitle, "SOL") ? Gold : Muted);
    rect(sy, 12, 58, 296, 1, Rule);
    rect(sy, 12, 58, 46, 2, f.accent);
    for (int i = 0; i < 6; i++) {
      int y = 70 + i * 21;
      bool selected = f.selected == i;
      if (selected) {
        rect(sy, 12, y - 3, 296, 20, Rule);
        rect(sy, 12, y - 3, 3, 20, Cyan);
        text(sy, 19, y, ">", 2, Cyan);
      }
      // Long informational rows stay within the screen instead of clipping.
      int scale = std::strlen(f.rows[i]) <= 23 ? 2 : 1;
      text(sy, 32, y + (scale == 1 ? 3 : 0), f.rows[i], scale,
           std::strstr(f.rows[i], "SOL") ? Gold : Ink);
    }
    rect(sy, 12, 197, 296, 1, Rule);
    text(sy, 14, 205, f.status, 1, f.accent);
    text(sy, 14, 226, f.footer, 1, Muted);
  }
};
} // namespace board::display
