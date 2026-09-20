#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace bm {
constexpr unsigned MaxPlayers = 12, MaxCoins = 16, MaxHoldings = 4,
                   MaxSave = 2048;
constexpr uint32_t WalletCap = 1000000000;
struct Mac {
  uint8_t b[6]{};
  bool operator==(const Mac &o) const { return !std::memcmp(b, o.b, 6); }
  bool operator!=(const Mac &o) const { return !(*this == o); }
};
enum class Op : uint8_t { Create = 1, Buy, Sell, Withdraw, Ticket, Claim };
enum class Error : uint8_t {
  Ok,
  Full,
  Player,
  Sequence,
  Funds,
  Amount,
  Holdings,
  Owned,
  Closed,
  Creator,
  Cooldown,
  Active,
  Symbol,
  Wait,
  Proof,
  Budget,
  Exhausted
};
const char *error_text(Error);
struct Request {
  uint32_t seq{};
  Op op{};
  uint8_t coin{};
  uint16_t amount{};
  char symbol[6]{};
  uint8_t proof[16]{};
};
struct Result {
  Error code{};
  uint8_t coin{};
  uint32_t ticket{};
};
struct Holding {
  uint8_t coin{};
  uint16_t quantity{}, eligible{}, since{};
};
struct Player {
  Mac mac{};
  uint32_t balance = 25000, next = 1, lastSeq{}, lastHash{}, ticket{};
  uint64_t gameAt{};
  uint16_t cooldown{}, budget{}, rugs{};
  uint8_t rep = 75;
  Result last{};
  Holding holdings[MaxHoldings]{};
};
struct Coin {
  char symbol[6]{};
  uint8_t creator{};
  bool rugged{};
  uint16_t supply{}, meme{}, seen{}, created{};
  uint32_t reserve{}, creatorFees{}, communityFees{};
};
struct Market {
  uint32_t id{}, revision = 1;
  uint16_t epoch = 1;
  uint8_t players{}, coins{};
  bool dirty{};
  Player p[MaxPlayers]{};
  Coin c[MaxCoins]{};
  void reset(uint32_t seed);
  int join(Mac);
  int find(Mac) const;
  Result request(uint8_t player, const Request &, uint64_t now,
                 uint32_t entropy);
  void advance_epoch();
  bool valid() const;
};
uint32_t crc32(const uint8_t *, size_t);
int64_t cost(int supply, int amount);
uint8_t command(uint32_t seed, unsigned round, unsigned &delay);
uint32_t request_hash(const Request &);
size_t encode(const Market &, uint8_t *, size_t);
bool decode(Market &, const uint8_t *, size_t, bool restore = false);
uint16_t owned(const Player &, unsigned coin);
// Explicit little-endian serialization, never memcpy native structs onto the
// wire.
struct Writer {
  uint8_t *b;
  size_t cap, pos = 0;
  bool ok = true;
  void u8(uint8_t x) {
    if (pos < cap)
      b[pos++] = x;
    else
      ok = false;
  }
  void u16(uint16_t x) {
    u8(x);
    u8(x >> 8);
  }
  void u32(uint32_t x) {
    u16(x);
    u16(x >> 16);
  }
  void bytes(const void *p, size_t n) {
    auto s = static_cast<const uint8_t *>(p);
    while (n--)
      u8(*s++);
  }
};
struct Reader {
  const uint8_t *b;
  size_t size, pos = 0;
  bool ok = true;
  uint8_t u8() {
    if (pos < size)
      return b[pos++];
    ok = false;
    return 0;
  }
  uint16_t u16() {
    auto a = u8();
    return a | (uint16_t(u8()) << 8);
  }
  uint32_t u32() {
    auto a = u16();
    return a | (uint32_t(u16()) << 16);
  }
  void bytes(void *p, size_t n) {
    auto s = static_cast<uint8_t *>(p);
    while (n--)
      *s++ = u8();
  }
};
} // namespace bm
