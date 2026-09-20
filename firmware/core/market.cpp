#include "market.hpp"
#include <algorithm>
#include <climits>
namespace bm {
const char *error_text(Error e) {
  static const char *const names[] = {
      "TRADE CONFIRMED",      "MARKET FULL",       "UNKNOWN PLAYER",
      "SYNC REQUIRED",        "NOT ENOUGH SOL",    "INVALID AMOUNT",
      "4 HOLDINGS MAX",       "NOT ENOUGH TOKENS", "COIN CLOSED",
      "CREATOR ONLY",         "RUG COOLDOWN",      "ONE ACTIVE COIN",
      "SYMBOL TAKEN/INVALID", "WAIT 20 SECONDS",   "INVALID GAME PROOF",
      "EPOCH REWARD CAP",     "SESSION EXHAUSTED"};
  auto i = unsigned(e);
  return i < 17 ? names[i] : "UNKNOWN ERROR";
}
uint32_t crc32(const uint8_t *s, size_t n) {
  uint32_t c = ~0u;
  while (n--) {
    c ^= *s++;
    for (int i = 0; i < 8; i++)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
  }
  return ~c;
}
int64_t cost(int q, int n) {
  if (q < 0 || n < 1 || n > 10000 || q + n > 10000)
    return -1;
  return int64_t(n) * 100 + 5LL * n * (2 * q + n - 1) / 2;
}
uint8_t command(uint32_t s, unsigned r, unsigned &d) {
  for (unsigned i = 0; i < r; i++)
    s = 1664525u * s + 1013904223u;
  d = 400 + (s % 5) * 100;
  return (s >> 16) % 3;
}
uint32_t request_hash(const Request &r) {
  uint8_t b[26];
  Writer w{b, sizeof b};
  w.u8(uint8_t(r.op));
  w.u8(r.coin);
  w.u16(r.amount);
  w.bytes(r.symbol, 6);
  w.bytes(r.proof, 16);
  return crc32(b, w.pos);
}
static Holding *holding(Player &p, unsigned coin) {
  Holding *empty = nullptr;
  for (auto &h : p.holdings) {
    if (h.coin == coin)
      return &h;
    if (!h.coin && !empty)
      empty = &h;
  }
  return empty;
}
uint16_t owned(const Player &p, unsigned coin) {
  for (auto &h : p.holdings)
    if (h.coin == coin)
      return h.quantity;
  return 0;
}
void Market::reset(uint32_t seed) {
  *this = Market{};
  id = seed ? seed : 1;
  dirty = true;
}
int Market::find(Mac m) const {
  for (int i = 0; i < players; i++)
    if (p[i].mac == m)
      return i;
  return -1;
}
int Market::join(Mac m) {
  int i = find(m);
  if (i >= 0)
    return i;
  if (players == MaxPlayers)
    return -1;
  p[players] = Player{};
  p[players].mac = m;
  dirty = true;
  revision++;
  return players++;
}
static bool symbol_ok(const char *s) {
  size_t n = 0;
  while (n < 6 && s[n])
    n++;
  if (n < 2 || n > 5 || s[0] < 'A' || s[0] > 'Z')
    return false;
  for (size_t i = 1; i < n; i++)
    if ((s[i] < 'A' || s[i] > 'Z') && (s[i] < '0' || s[i] > '9'))
      return false;
  return true;
}
Result Market::request(uint8_t pid, const Request &r, uint64_t now,
                       uint32_t entropy) {
  if (pid >= players)
    return {Error::Player, 0, 0};
  auto &a = p[pid];
  auto hash = request_hash(r);
  if (r.seq == a.lastSeq && r.seq)
    return hash == a.lastHash ? a.last : Result{Error::Sequence, 0, 0};
  if (r.seq != a.next)
    return {Error::Sequence, 0, 0};
  if (r.seq == UINT32_MAX || revision >= UINT32_MAX - 1)
    return {Error::Exhausted, 0, 0};
  Result result{};
  auto fail = [&](Error e) { result.code = e; };
  Coin *coin = r.coin && r.coin <= coins ? &c[r.coin - 1] : nullptr;
  switch (r.op) {
  case Op::Create:
    if (!symbol_ok(r.symbol)) {
      fail(Error::Symbol);
      break;
    }
    if (coins == MaxCoins) {
      fail(Error::Full);
      break;
    }
    if (epoch < a.cooldown) {
      fail(Error::Cooldown);
      break;
    }
    if (a.balance < 2000) {
      fail(Error::Funds);
      break;
    }
    for (unsigned i = 0; i < coins; i++) {
      if (c[i].creator == pid && !c[i].rugged)
        fail(Error::Active);
      if (!std::strcmp(c[i].symbol, r.symbol))
        fail(Error::Symbol);
    }
    if (result.code == Error::Ok) {
      auto &v = c[coins++];
      v = Coin{};
      std::memcpy(v.symbol, r.symbol, 6);
      v.creator = pid;
      v.created = epoch;
      a.balance -= 2000;
      result.coin = coins;
    }
    break;
  case Op::Buy:
  case Op::Sell: {
    bool buy = r.op == Op::Buy;
    int n = r.amount;
    if (!coin || (buy && coin->rugged)) {
      fail(Error::Closed);
      break;
    }
    auto *h = holding(a, r.coin);
    int q = h ? h->quantity : 0;
    auto gross = cost(buy ? coin->supply : coin->supply - n, n);
    if (!buy && q < n) {
      fail(Error::Owned);
      break;
    }
    if (gross < 0) {
      fail(Error::Amount);
      break;
    }
    uint32_t fee = coin->rugged ? 0 : (gross + 49) / 50;
    if (uint64_t(coin->creatorFees) + coin->communityFees + fee > WalletCap) {
      fail(Error::Amount);
      break;
    }
    if (buy && !h) {
      fail(Error::Holdings);
      break;
    }
    if (buy && a.balance < gross + fee) {
      fail(Error::Funds);
      break;
    }
    if (!buy &&
        (coin->reserve < gross || a.balance + gross - fee > WalletCap)) {
      fail(Error::Amount);
      break;
    }
    if (buy) {
      a.balance -= gross + fee;
      coin->reserve += gross;
      coin->supply += n;
      if (!q) {
        h->since = epoch;
        h->eligible = 0;
      }
      h->coin = r.coin;
      h->quantity = q + n;
      if (!(coin->seen & (1 << pid))) {
        coin->seen |= 1 << pid;
        coin->meme = std::min(1000, int(coin->meme) + 10);
      }
    } else {
      a.balance += gross - fee;
      coin->reserve -= gross;
      coin->supply -= n;
      h->quantity = q - n;
      h->eligible = std::min(h->eligible, h->quantity);
      h->since = epoch;
      if (!h->quantity)
        *h = Holding{};
    }
    coin->creatorFees += fee / 2;
    coin->communityFees += fee - fee / 2;
    result.coin = r.coin;
    break;
  }
  case Op::Withdraw: {
    int percent = r.amount;
    if (!coin || coin->rugged) {
      fail(Error::Closed);
      break;
    }
    if (coin->creator != pid) {
      fail(Error::Creator);
      break;
    }
    if (percent != 10 && percent != 25 && percent != 50 && percent != 100) {
      fail(Error::Amount);
      break;
    }
    uint32_t amount = percent == 100
                          ? coin->creatorFees + coin->communityFees
                          : uint64_t(coin->creatorFees) * percent / 100;
    if (uint64_t(a.balance) + amount > WalletCap) {
      fail(Error::Amount);
      break;
    }
    a.balance += amount;
    if (percent == 100) {
      coin->creatorFees = coin->communityFees = 0;
      coin->rugged = true;
      coin->meme = 0;
      a.rep = a.rep > 35 ? a.rep - 35 : 0;
      a.rugs++;
      a.cooldown = std::min(65535, int(epoch) + 3);
    } else
      coin->creatorFees -= amount;
    result.coin = r.coin;
    break;
  }
  case Op::Ticket:
    if (a.gameAt && now - a.gameAt < 20000) {
      fail(Error::Wait);
      break;
    }
    if (a.budget >= 6000) {
      fail(Error::Budget);
      break;
    }
    a.ticket = entropy ? entropy : 1;
    a.gameAt = now ? now : 1;
    result.ticket = a.ticket;
    break;
  case Op::Claim: {
    uint64_t elapsed = now >= a.gameAt ? now - a.gameAt : 0;
    unsigned score = 0, total = 0;
    if (!a.ticket || elapsed < 4000 || elapsed > 45000)
      fail(Error::Proof);
    for (unsigned i = 0; i < 8; i++) {
      unsigned delay;
      auto key = command(a.ticket, i + 1, delay);
      unsigned dt = r.proof[i + 8] * 20;
      auto pressed = r.proof[i];
      if (pressed > 3 || dt < 100 || dt > 2400)
        fail(Error::Proof);
      if (pressed == key && dt >= 100 && dt <= 1800)
        score += 250;
      total += delay + dt;
    }
    if (total > elapsed + 1000)
      fail(Error::Proof);
    if (result.code == Error::Ok) {
      uint32_t reward =
          std::min<uint32_t>({score, 6000u - a.budget, WalletCap - a.balance});
      a.balance += reward;
      a.budget += reward;
    }
    a.ticket = 0;
    break;
  }
  default:
    fail(Error::Amount);
    break;
  }
  a.lastSeq = r.seq;
  a.next = r.seq + 1;
  a.lastHash = hash;
  a.last = result;
  dirty = true;
  revision++;
  return result;
}
void Market::advance_epoch() {
  if (epoch >= 65000)
    return;
  epoch++;
  for (unsigned j = 0; j < coins; j++) {
    auto &coin = c[j];
    uint32_t weight = 0;
    for (unsigned i = 0; i < players; i++)
      for (auto &h : p[i].holdings)
        if (h.coin == j + 1)
          weight +=
              h.eligible *
              std::min(125, 100 + std::max(0, int(epoch) - h.since - 2) * 10);
    uint32_t pool = coin.rugged ? 0 : coin.communityFees / 2;
    for (unsigned i = 0; i < players; i++)
      for (auto &h : p[i].holdings)
        if (h.coin == j + 1) {
          uint32_t reward =
              weight
                  ? uint64_t(pool) * h.eligible *
                        std::min(125,
                                 100 + std::max(0, int(epoch) - h.since - 2) *
                                           10) /
                        weight
                  : 0;
          reward = std::min(reward, WalletCap - p[i].balance);
          p[i].balance += reward;
          coin.communityFees -= reward;
          h.eligible = h.quantity;
        }
    coin.meme = coin.meme * 95 / 100;
    if (!coin.rugged && coin.supply && (epoch - coin.created) % 3 == 0)
      p[coin.creator].rep = std::min(100, int(p[coin.creator].rep) + 1);
  }
  for (unsigned i = 0; i < players; i++) {
    p[i].budget = 0;
    p[i].ticket = 0;
    p[i].gameAt = 0;
  }
  dirty = true;
  revision++;
}
bool Market::valid() const {
  if (!id || !epoch || epoch > 65000 || players > MaxPlayers ||
      coins > MaxCoins)
    return false;
  for (unsigned i = 0; i < players; i++) {
    auto &a = p[i];
    if (a.balance > WalletCap || !a.next || a.rep > 100 || a.budget > 6000 ||
        unsigned(a.last.code) > 16 || a.last.coin > coins ||
        a.lastSeq >= a.next)
      return false;
    for (unsigned k = 0; k < i; k++)
      if (p[k].mac == a.mac)
        return false;
    for (unsigned j = 0; j < MaxHoldings; j++) {
      auto &h = a.holdings[j];
      if (h.coin > coins || h.quantity > 10000 || h.eligible > h.quantity ||
          h.since > epoch || (!h.coin && h.quantity) || (h.coin && !h.quantity))
        return false;
      for (unsigned k = 0; k < j; k++)
        if (h.coin && a.holdings[k].coin == h.coin)
          return false;
    }
  }
  for (unsigned j = 0; j < coins; j++) {
    auto &coin = c[j];
    if (!symbol_ok(coin.symbol) || coin.creator >= players ||
        coin.supply > 10000 || coin.meme > 1000 || coin.created > epoch ||
        coin.reserve != (coin.supply ? cost(0, coin.supply) : 0) ||
        uint64_t(coin.creatorFees) + coin.communityFees > WalletCap)
      return false;
    for (unsigned k = 0; k < j; ++k)
      if (!std::strcmp(coin.symbol, c[k].symbol))
        return false;
    unsigned supply = 0;
    for (unsigned i = 0; i < players; i++)
      supply += owned(p[i], j + 1);
    if (supply != coin.supply)
      return false;
  }
  return true;
}
size_t encode(const Market &m, uint8_t *b, size_t cap) {
  Writer w{b, cap};
  w.u32(0x324d424e);
  w.u32(m.id);
  w.u32(m.revision);
  w.u16(m.epoch);
  w.u8(m.players);
  w.u8(m.coins);
  for (unsigned i = 0; i < m.players; i++) {
    auto &p = m.p[i];
    w.bytes(p.mac.b, 6);
    w.u32(p.balance);
    w.u32(p.next);
    w.u32(p.lastSeq);
    w.u32(p.lastHash);
    w.u8(p.rep);
    w.u16(p.rugs);
    w.u16(p.cooldown);
    w.u16(p.budget);
    w.u8(uint8_t(p.last.code));
    w.u8(p.last.coin);
    w.u32(p.last.ticket);
    w.u32(p.ticket);
    for (auto &h : p.holdings) {
      w.u8(h.coin);
      w.u16(h.quantity);
      w.u16(h.eligible);
      w.u16(h.since);
    }
  }
  for (unsigned i = 0; i < m.coins; i++) {
    auto &c = m.c[i];
    w.bytes(c.symbol, 6);
    w.u8(c.creator);
    w.u8(c.rugged);
    w.u16(c.supply);
    w.u16(c.meme);
    w.u16(c.seen);
    w.u16(c.created);
    w.u32(c.reserve);
    w.u32(c.creatorFees);
    w.u32(c.communityFees);
  }
  auto crc = crc32(b, w.pos);
  w.u32(crc);
  return w.ok ? w.pos : 0;
}
bool decode(Market &out, const uint8_t *b, size_t n, bool restore) {
  if (n < 20 || n > MaxSave)
    return false;
  Reader tail{b + n - 4, 4};
  if (crc32(b, n - 4) != tail.u32())
    return false;
  Reader r{b, n - 4};
  if (r.u32() != 0x324d424e)
    return false;
  Market m{};
  m.id = r.u32();
  m.revision = r.u32();
  m.epoch = r.u16();
  m.players = r.u8();
  m.coins = r.u8();
  if (m.players > MaxPlayers || m.coins > MaxCoins)
    return false;
  for (unsigned i = 0; i < m.players; i++) {
    auto &p = m.p[i];
    r.bytes(p.mac.b, 6);
    p.balance = r.u32();
    p.next = r.u32();
    p.lastSeq = r.u32();
    p.lastHash = r.u32();
    p.rep = r.u8();
    p.rugs = r.u16();
    p.cooldown = r.u16();
    p.budget = r.u16();
    p.last.code = Error(r.u8());
    p.last.coin = r.u8();
    p.last.ticket = r.u32();
    p.ticket = r.u32();
    for (auto &h : p.holdings) {
      h.coin = r.u8();
      h.quantity = r.u16();
      h.eligible = r.u16();
      h.since = r.u16();
    }
    if (restore) {
      p.ticket = 0;
      p.last.ticket = 0;
    }
  }
  for (unsigned i = 0; i < m.coins; i++) {
    auto &c = m.c[i];
    r.bytes(c.symbol, 6);
    c.creator = r.u8();
    auto rugged = r.u8();
    if (rugged > 1)
      return false;
    c.rugged = rugged;
    c.supply = r.u16();
    c.meme = r.u16();
    c.seen = r.u16();
    c.created = r.u16();
    c.reserve = r.u32();
    c.creatorFees = r.u32();
    c.communityFees = r.u32();
  }
  if (!r.ok || r.pos != n - 4 || !m.valid())
    return false;
  out = m;
  return true;
}
} // namespace bm
