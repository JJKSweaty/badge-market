#pragma once
#include <algorithm>
#include <cstdint>
namespace bm {
enum class Glow {
  None,
  Navigate,
  Buy,
  Sell,
  Launch,
  Reward,
  Error,
  Rug,
  Win,
  WordSolved,
  Save
};
struct Light {
  uint8_t r{}, g{}, b{};
  int active = -1;
};
// One bounded transient, no queues/allocations. Colors are intentionally dim.
class Feedback {
public:
  Glow effect = Glow::None;
  uint64_t at{}, until{};
  void trigger(Glow kind, uint64_t now) {
    if (now < until &&
        (effect == Glow::Rug || kind == Glow::Navigate || kind == Glow::Save))
      return;
    effect = kind;
    at = now;
    until = now + (kind == Glow::Navigate ? 120
                   : kind == Glow::Rug    ? 1000
                   : kind == Glow::Win    ? 1200
                                          : 800);
  }
  Light sample(uint64_t now) const {
    if (now >= until)
      return {};
    unsigned elapsed = unsigned(now - at);
    switch (effect) {
    case Glow::Navigate:
      return {2, 5, 7, int((now / 120) % 6)};
    case Glow::Buy:
      return elapsed < 350 ? Light{0, 12, 2, -1} : Light{0, 9, 12, -1};
    case Glow::Sell: {
      static constexpr int outward[] = {2, 3, 1, 4, 0, 5};
      return {12, 1, 2, outward[std::min(5u, elapsed / 100)]};
    }
    case Glow::WordSolved:
      return elapsed < 400 ? Light{0, 12, 3, -1}
                           : Light{12, 8, 0, int(elapsed / 100 % 6)};
    case Glow::Launch:
      return {9, 2, 12, int(elapsed / 100 % 6)};
    case Glow::Reward:
      return {12, 8, 0, int(elapsed / 120 % 6)};
    case Glow::Win:
      return elapsed < 400   ? Light{12, 9, 0, -1}
             : elapsed < 800 ? Light{0, 10, 12, -1}
                             : Light{0, 12, 3, -1};
    case Glow::Error:
      return {12, 0, 0, -1};
    case Glow::Rug:
      return {uint8_t(elapsed % 160 < 80 ? 12 : 0), 0, 0, -1};
    case Glow::Save:
      return {0, 8, 7, -1};
    default:
      return {};
    }
  }
};
inline Light brightness(Light f, bool enabled, bool dim) {
  if (!enabled)
    return {};
  if (dim) {
    f.r = (f.r + 3) / 4;
    f.g = (f.g + 3) / 4;
    f.b = (f.b + 3) / 4;
  }
  return f;
}
} // namespace bm
