#include "play.hpp"
#include <algorithm>
#include <cstring>
namespace bm {
uint32_t random_step(uint32_t &s) {
  s = s * 1664525u + 1013904223u;
  return s;
}
// Curated five-byte flash records; no pointers, per-word allocations or RAM
// copy. The generated include is sorted and validated by tools/build_words.py.
#include "words.inc"
size_t answer_count() { return sizeof(wordAnswers) / 5; }
void answer_word(uint32_t seed, char out[6]) {
  std::memcpy(out, wordAnswers + (seed % answer_count()) * 5, 5);
  out[5] = 0;
}
bool valid_word(const char *w) {
  for (int i = 0; i < 5; ++i)
    if (w[i] < 'A' || w[i] > 'Z')
      return false;
  size_t lo = 0, hi = sizeof(wordGuesses) / 5;
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    int c = std::memcmp(w, wordGuesses + mid * 5, 5);
    if (!c)
      return true;
    if (c < 0)
      hi = mid;
    else
      lo = mid + 1;
  }
  return false;
}
void evaluate_word(const char *a, const char *g, uint8_t out[5]) {
  uint8_t counts[26]{};
  for (int i = 0; i < 5; i++) {
    out[i] = a[i] == g[i] ? 2 : 0;
    if (!out[i])
      counts[a[i] - 'A']++;
  }
  for (int i = 0; i < 5; i++)
    if (!out[i] && g[i] >= 'A' && g[i] <= 'Z' && counts[g[i] - 'A']) {
      out[i] = 1;
      --counts[g[i] - 'A'];
    }
}
void MemeWord::start(uint32_t seed) {
  *this = MemeWord{};
  answer_word(seed, answer);
}
void MemeWord::input(int k, uint64_t now) {
  if (done || (revealAt && now < revealAt + 700))
    return;
  // Native button indices: A B HOME DOWN LEFT RIGHT UP AUX START.
  char &v = guesses[row][cursor];
  if (k == 4 && cursor)
    --cursor;
  else if (k == 5 && cursor < 4)
    ++cursor;
  else if (k == 6)
    v = !v || v == 'Z' ? 'A' : v + 1;
  else if (k == 3)
    v = !v || v == 'A' ? 'Z' : v - 1;
  else if (k == 1)
    v = 0;
  else if (k == 0) {
    for (int i = 0; i < 5; i++)
      if (!guesses[row][i]) {
        message = "FILL ALL FIVE LETTERS";
        return;
      }
    if (!valid_word(guesses[row])) {
      message = "WORD NOT IN BADGE DICTIONARY";
      return;
    }
    evaluate_word(answer, guesses[row], marks[row]);
    solved = std::memcmp(answer, guesses[row], 5) == 0;
    ++row;
    cursor = 0;
    revealAt = now;
    done = solved || row == 6;
    message = done ? (solved ? "SOLVED!" : "NICE TRY") : "KEEP GOING";
  }
}
void Runner::start(uint32_t s) {
  *this = Runner{};
  seed = s;
}
bool Runner::input(int key, bool record) {
  if (done)
    return false;
  if (!((key == 4 && lane > 0 && x == targetX) ||
        (key == 5 && lane < 2 && x == targetX) || (key == 0 && !jump)))
    return false;
  if (record) {
    if (traceCount == MaxTrace)
      overflow = true;
    else {
      auto *p = trace + traceCount++ * 3;
      p[0] = tick;
      p[1] = tick >> 8;
      p[2] = key;
    }
  }
  if (key == 0)
    jump = 25;
  else {
    lane += key == 4 ? -1 : 1;
    targetX = 64 + 96 * lane;
  }
  return true;
}
void Runner::spawn() {
  // Adjacent safe lanes + >=900ms wave separation guarantee a no-jump route.
  int shift = int(random_step(seed) % 3) - 1;
  safeLane = std::clamp(int(safeLane) + shift, 0, 2);
  auto add = [&](int l, int kind, int y) {
    for (auto &o : objects)
      if (!o.active) {
        o = {int16_t(y), uint8_t(l), uint8_t(kind), true};
        return;
      }
  };
  for (int l = 0; l < 3; l++)
    if (l != safeLane)
      add(l, 1 + (random_step(seed) % 2), -16);
  add(safeLane, 3, -52);
  if (random_step(seed) % 4 == 0)
    add(safeLane, 4 + random_step(seed) % 3, -88);
  nextWave = tick + std::max(48, 80 - int(tick) / 65);
}
void Runner::step() {
  if (done)
    return;
  ++tick;
  if (jump)
    --jump;
  if (immune)
    --immune;
  if (magnet)
    --magnet;
  if (rocket)
    --rocket;
  x += std::clamp(int(targetX - x), -16, 16);
  if (tick >= nextWave)
    spawn();
  int speed = 2 + tick / 650;
  score += rocket ? 2 : 1;
  for (auto &p : particles)
    if (p.life) {
      --p.life;
      --p.y;
      p.x += p.life % 2 ? 1 : -1;
    }
  for (auto &o : objects)
    if (o.active) {
      o.y += speed;
      bool near = std::abs((64 + 96 * int(o.lane)) - int(x)) < 25;
      bool hit = o.y >= 174 && o.y <= 198;
      if (o.kind >= 3 && magnet && o.y >= 140 && o.y <= 198)
        near = true, hit = true;
      if (hit && near) {
        if (o.kind <= 2) {
          if (!(o.kind == 1 && jump) && !immune && !rocket) {
            if (shield)
              shield = false;
            else
              --hearts;
            immune = 40;
            hitAt = tick;
            pickups = 0;
            combo = 1;
          }
        } else {
          if (o.kind == 3) {
            ++pickups;
            combo = std::min(4, 1 + int(pickups) / 3);
            score += 30 * combo;
          }
          if (o.kind == 4)
            shield = true;
          if (o.kind == 5)
            magnet = 200;
          if (o.kind == 6)
            rocket = 100;
          collectAt = tick;
          for (int i = 0; i < 6; i++)
            particles[i] = {int16_t(x + (i - 3) * 3), 180, uint8_t(10 + i)};
        }
        o.active = false;
      }
      if (o.y > 242)
        o.active = false;
    }
  done = !hearts || tick >= Duration;
}
bool Runner::verify(uint32_t s, const uint8_t *data, size_t n, uint16_t ticks,
                    uint32_t expected) {
  if (n % 3 || n > MaxTrace * 3 || !ticks || ticks > Duration)
    return false;
  Runner r;
  r.start(s);
  size_t pos = 0;
  while (r.tick < ticks && !r.done) {
    while (pos < n) {
      auto at = uint16_t(data[pos] | uint16_t(data[pos + 1]) << 8);
      if (at < r.tick)
        return false;
      if (at != r.tick)
        break;
      if (!r.input(data[pos + 2], false))
        return false;
      pos += 3;
    }
    r.step();
  }
  return r.done && r.tick == ticks && pos == n && r.score == expected;
}
} // namespace bm
