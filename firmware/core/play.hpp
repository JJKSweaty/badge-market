#pragma once
#include <cstddef>
#include <cstdint>
namespace bm {
enum class Game : uint8_t { None, Word, Run, Bomb };
uint32_t random_step(uint32_t &seed);
size_t answer_count();
void answer_word(uint32_t seed, char out[6]);
bool valid_word(const char *word);
void evaluate_word(const char *answer, const char *guess, uint8_t out[5]);
struct MemeWord {
  char answer[6]{}, guesses[6][6]{};
  uint8_t marks[6][5]{}, row{}, cursor{};
  bool done{}, solved{};
  uint64_t revealAt{};
  const char *message = "BUILD A FIVE LETTER WORD";
  void start(uint32_t seed);
  void input(int key, uint64_t now);
};
struct Runner {
  static constexpr unsigned MaxTrace = 512, Duration = 2000;
  struct Object {
    int16_t y{};
    uint8_t lane{}, kind{};
    bool active{};
  };
  struct Particle {
    int16_t x{}, y{};
    uint8_t life{};
  };
  Object objects[28]{};
  Particle particles[12]{};
  uint8_t trace[MaxTrace * 3]{};
  uint16_t traceCount{}, tick{}, nextWave = 70, jump{}, immune{}, magnet{},
                                 rocket{};
  uint16_t pickups{}, combo = 1, hitAt{}, collectAt{};
  int16_t x = 160, targetX = 160;
  uint8_t lane = 1, hearts = 3, safeLane = 1;
  uint32_t seed{}, score{};
  bool shield{}, done{}, overflow{};
  void start(uint32_t s);
  bool input(int key, bool record = true);
  void step();
  void spawn();
  static bool verify(uint32_t seed, const uint8_t *trace, size_t size,
                     uint16_t ticks, uint32_t expected);
};
} // namespace bm
