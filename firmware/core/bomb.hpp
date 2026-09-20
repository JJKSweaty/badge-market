#pragma once
#include "session.hpp"
namespace bm {
struct Bomb {
  enum Phase : uint8_t { Off, Lobby, Countdown, Live, Over, Cancelled };
  struct Member {
    uint8_t player{}, passes{};
    bool ready{}, left{};
    uint32_t ack{}, command{};
    uint64_t seen{};
    uint16_t reward{};
  };
  Session *session{};
  Member members[6]{};
  Phase phase = Off;
  uint8_t count{}, holder = 255, previous = 255, target = 255, key{}, tension{};
  uint32_t id{}, round{}, revision{}, challenge{}, cmdSeq{};
  uint64_t deadline{}, phaseAt{}, lockUntil{}, challengeLimit{}, sendAt{},
      seenAt{}, ackAt{}, commandAt{};
  uint32_t privateSeed{};
  uint64_t retryUntil{};
  bool host{}, selecting{}, responded{}, changed{}, challengeAck{};
  int selection{};
  const char *message = "A CREATE GAME";
  uint8_t commandKind{};
  uint8_t commandData[16]{};
  size_t commandSize{};
  uint32_t lastResponseChallenge{};
  void enter(Session &, uint64_t);
  void exit(uint64_t);
  void create(uint64_t);
  void input(int, uint64_t);
  void tick(uint64_t);
  void receive(Mac, uint8_t, uint32_t, const uint8_t *, size_t, uint64_t);
  void announce(uint64_t);
  void send_state();
  void command(uint8_t, const uint8_t *, size_t, uint64_t);
  void process(unsigned, uint8_t, const uint8_t *, size_t, uint64_t);
  int member(unsigned) const;
  bool mine() const;
};
} // namespace bm
