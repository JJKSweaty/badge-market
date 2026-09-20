#pragma once
#include "session.hpp"
namespace bm {
struct Reaction {
  enum Phase { Idle, Pause, Wait, Go, Done };
  Phase phase = Idle;
  unsigned round{}, score{};
  uint8_t key{}, proof[16]{};
  uint32_t seed{};
  uint64_t deadline{}, at{};
  void start(uint32_t s, uint64_t now);
  bool tick(uint64_t now);
  void press(uint8_t k, uint64_t now);
};
struct Duel {
  enum Phase { Idle, Offer, Invite, Accept, Wait, Go, Result, Done, Cancelled };
  Phase phase = Idle;
  Session *session{};
  Mac peer{};
  uint32_t nonce{}, seed{};
  uint16_t mine = 65535, theirs = 65535;
  bool haveMine{}, haveTheirs{}, changed{};
  uint64_t at{}, limit{}, sendAt{};
  uint8_t packet{};
  uint32_t value{};
  void init(Session &);
  void challenge(Mac, uint32_t, uint64_t);
  void press(bool accept, uint64_t);
  void tick(uint64_t);
  void receive(Mac, uint8_t, uint32_t, uint32_t, uint64_t);
  void queue(uint8_t, uint32_t, uint64_t);
};
} // namespace bm
