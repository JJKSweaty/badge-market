#pragma once
#include "market.hpp"
namespace bm {
constexpr size_t PacketMax = 250, Chunk = 200;
enum class Mode : uint8_t { Menu, Solo, Host, Client };
struct Peer {
  Mac mac{};
  uint32_t market{};
  uint64_t seen{};
  int8_t rssi{};
};
using Send = bool (*)(void *, const uint8_t *, size_t);
using Entropy = uint32_t (*)(void *);
using Social = void (*)(void *, Mac, uint8_t, uint32_t, uint32_t, uint64_t);
class Session {
public:
  Market world{};
  Mode mode = Mode::Menu;
  Mac self{}, host{};
  int player = -1;
  Peer peers[8]{};
  uint8_t peerCount{};
  bool pending{}, changed = true;
  Result last{};
  uint32_t completed{};
  Op completedOp{};
  const char *status = "CHOOSE A MARKET";
  uint64_t lastSeen{};
  uint32_t malformed{};
  Send send{};
  Entropy entropy{};
  void *context{};
  using GameReceive = void (*)(void *, Mac, uint8_t, uint32_t, const uint8_t *,
                               size_t, uint64_t);
  GameReceive gameReceive{};
  void *gameContext{};
  bool send_game(Mac, uint8_t, uint32_t, const uint8_t *, size_t);
  Social social{};
  void *socialContext{};
  void init(Mac, Send, Entropy, void *);
  void start(bool hosting, uint64_t now, const Market *restored = nullptr);
  bool join(unsigned index, uint64_t now);
  void cancel_join();
  bool act(Request, uint64_t now);
  void tick(uint64_t now);
  void receive(Mac from, int8_t rssi, const uint8_t *, size_t, uint64_t now);
  bool send_social(Mac target, uint8_t kind, uint32_t nonce, uint32_t value);
  bool authority() const { return mode == Mode::Host || mode == Mode::Solo; }

private:
  Request request_{};
  uint32_t market_{}, rxRevision_{}, rxMask_{};
  uint16_t rxSize_{};
  uint8_t rx_[MaxSave]{}, tx_[MaxSave]{};
  uint16_t txSize_{}, txOffset_{};
  uint32_t txRevision_{};
  uint64_t retryAt_{}, beaconAt_{}, syncAt_{}, epochAt_{}, txAt_{};
  bool frame(uint8_t kind, Mac target, uint32_t serial, const uint8_t *,
             size_t);
  void queue_snapshot();
};
} // namespace bm
