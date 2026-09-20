#include "games.hpp"
#include "market.hpp"
#include "session.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>
using namespace bm;
static Mac mac(unsigned n) {
  return Mac{{2, 0, 0, 0, uint8_t(n >> 8), uint8_t(n)}};
}
static Result act(Market &m, int p, Op op, int coin = 0, int amount = 0,
                  const char *symbol = "COIN", uint64_t now = 1000) {
  Request r{};
  r.seq = m.p[p].next;
  r.op = op;
  r.coin = coin;
  r.amount = amount;
  snprintf(r.symbol, 6, "%s", symbol);
  return m.request(p, r, now, 123456);
}
static uint64_t assets(const Market &m) {
  uint64_t sum = 0;
  for (unsigned i = 0; i < m.players; i++)
    sum += m.p[i].balance;
  for (unsigned i = 0; i < m.coins; i++)
    sum += uint64_t(m.c[i].reserve) + m.c[i].creatorFees + m.c[i].communityFees;
  return sum;
}
struct Message {
  int from;
  std::vector<uint8_t> bytes;
};
static std::vector<Message> messages;
struct Node {
  Session s;
  int id;
  uint32_t rng = 71;
  Duel duel;
  Node(int n) : id(n) {
    s.init(
        mac(n),
        [](void *p, const uint8_t *b, size_t size) {
          auto *node = static_cast<Node *>(p);
          messages.push_back({node->id, {b, b + size}});
          return true;
        },
        [](void *p) {
          auto *node = static_cast<Node *>(p);
          node->rng = node->rng * 1664525 + 1013904223;
          return node->rng;
        },
        this);
    duel.init(s);
  }
};
int main() {
  Market m;
  m.reset(123);
  assert(m.join(mac(1)) == 0);
  assert(m.join(mac(2)) == 1);
  assert(m.join(mac(1)) == 0);
  assert(cost(0, 1) == 100);
  assert(cost(10, 5) == 800);
  assert(cost(-1, 2) == -1);
  assert(cost(0, 10001) == -1);
  assert(act(m, 0, Op::Create).code == Error::Ok);
  auto total = assets(m);
  assert(total == 48000);
  Request buy{};
  buy.seq = m.p[1].next;
  buy.op = Op::Buy;
  buy.coin = 1;
  buy.amount = 10;
  auto result = m.request(1, buy, 1000, 1);
  assert(result.code == Error::Ok);
  auto balance = m.p[1].balance;
  assert(m.request(1, buy, 1000, 1).code == Error::Ok &&
         m.p[1].balance == balance);
  buy.amount = 11;
  assert(m.request(1, buy, 1000, 1).code == Error::Sequence);
  assert(assets(m) == total);
  assert(m.valid());
  m.advance_epoch();
  auto before = m.p[1].balance;
  m.advance_epoch();
  assert(m.p[1].balance > before);
  assert(assets(m) == total);
  assert(act(m, 1, Op::Withdraw, 1, 100).code == Error::Creator);
  assert(act(m, 0, Op::Withdraw, 1, 100).code == Error::Ok);
  assert(act(m, 1, Op::Buy, 1, 1).code == Error::Closed);
  assert(act(m, 1, Op::Sell, 1, 10).code == Error::Ok);
  assert(m.c[0].supply == 0 && m.c[0].reserve == 0);
  assert(assets(m) == total && m.valid());
  uint8_t data[MaxSave];
  auto n = encode(m, data, sizeof data);
  assert(n > 0);
  Market restored;
  assert(decode(restored, data, n, true));
  assert(restored.valid());
  auto old = restored.revision;
  data[20] ^= 1;
  assert(!decode(restored, data, n));
  assert(restored.revision == old);
  data[20] ^= 1;
  assert(!decode(restored, data, n - 1));
  Market trades;
  trades.reset(42);
  for (int i = 1; i <= 12; i++)
    assert(trades.join(mac(i)) == i - 1);
  assert(trades.join(mac(13)) == -1);
  for (int i = 0; i < 12; i++) {
    char s[6];
    snprintf(s, 6, "C%03d", i);
    assert(act(trades, i, Op::Create, 0, 0, s).code == Error::Ok);
  }
  total = assets(trades);
  std::mt19937 random(12);
  for (int i = 0; i < 30000; i++) {
    int p = random() % 12, c = random() % 12 + 1, q = random() % 10 + 1;
    act(trades, p, random() % 2 ? Op::Buy : Op::Sell, c, q);
    if (i % 99 == 0)
      trades.advance_epoch();
    assert(trades.valid());
    assert(assets(trades) == total);
  }
  n = encode(trades, data, sizeof data);
  assert(n && decode(restored, data, n));
  for (unsigned i = 0; i < trades.players; i++)
    assert(restored.p[i].balance == trades.p[i].balance);
  for (int i = 0; i < 10000; i++) {
    size_t length = random() % MaxSave;
    for (size_t j = 0; j < length; j++)
      data[j] = random();
    assert(!decode(restored, data, length));
  }
  Market capacity;
  capacity.reset(19);
  for (unsigned i = 0; i < 12; i++) {
    capacity.join(mac(i + 1));
    char s[6];
    snprintf(s, 6, "X%03u", i);
    assert(act(capacity, i, Op::Create, 0, 0, s).code == Error::Ok);
  }
  for (unsigned i = 0; i < 4; i++)
    assert(act(capacity, i, Op::Withdraw, i + 1, 100).code == Error::Ok);
  for (int i = 0; i < 3; i++)
    capacity.advance_epoch();
  for (unsigned i = 0; i < 4; i++) {
    char s[6];
    snprintf(s, 6, "Y%03u", i);
    assert(act(capacity, i, Op::Create, 0, 0, s).code == Error::Ok);
  }
  assert(capacity.coins == 16 && capacity.valid());
  assert(act(capacity, 5, Op::Create, 0, 0, "LIMIT").code == Error::Full);
  n = encode(capacity, data, sizeof data);
  assert(n && decode(restored, data, n));
  // Exercise structurally malformed payloads with a correct CRC as well.
  for (int i = 0; i < 10000; i++) {
    encode(capacity, data, sizeof data);
    data[random() % (n - 4)] ^= uint8_t(1 + random() % 255);
    Writer crc{data + n - 4, 4};
    crc.u32(crc32(data, n - 4));
    if (decode(restored, data, n))
      assert(restored.valid());
  }
  Market reward;
  reward.reset(99);
  reward.join(mac(1));
  auto ticket = act(reward, 0, Op::Ticket, 0, 0, "COIN", 1000);
  assert(ticket.ticket);
  assert(act(reward, 0, Op::Ticket, 0, 0, "COIN", 1001).code == Error::Wait);
  Request claim{};
  claim.seq = reward.p[0].next;
  claim.op = Op::Claim;
  for (unsigned i = 0; i < 8; i++) {
    unsigned delay;
    claim.proof[i] = command(ticket.ticket, i + 1, delay);
    claim.proof[i + 8] = 10;
  }
  assert(reward.request(0, claim, 12000, 1).code == Error::Ok);
  assert(reward.p[0].balance == 27000);
  assert(reward.request(0, claim, 12000, 1).code == Error::Ok &&
         reward.p[0].balance == 27000);
  Reaction game;
  game.start(123, 0);
  game.tick(800);
  assert(game.phase == Reaction::Wait);
  game.press(game.key, 900);
  assert(game.score == 0 && game.proof[0] == 3);
  Node host(1), client(2);
  host.s.start(true, 0);
  uint64_t clock = 0;
  int dropped = 0, duplicated = 0;
  auto step = [&](unsigned duration, bool loss) {
    for (unsigned t = 0; t < duration; t += 20) {
      clock += 20;
      host.s.tick(clock);
      client.s.tick(clock);
      host.duel.tick(clock);
      client.duel.tick(clock);
      auto queue = std::move(messages);
      messages.clear();
      if (loss && random() % 2)
        std::reverse(queue.begin(), queue.end());
      for (auto &msg : queue) {
        if (loss && random() % 4 == 0) {
          dropped++;
          continue;
        }
        auto &target = msg.from == 1 ? client.s : host.s;
        target.receive(mac(msg.from), -40, msg.bytes.data(), msg.bytes.size(),
                       clock);
        if (loss && random() % 3 == 0) {
          target.receive(mac(msg.from), -40, msg.bytes.data(), msg.bytes.size(),
                         clock);
          duplicated++;
        }
      }
    }
  };
  step(5000, true);
  assert(client.s.peerCount == 1);
  assert(client.s.join(0, clock));
  step(12000, true);
  assert(client.s.player == 1);
  Request create{};
  create.op = Op::Create;
  std::memcpy(create.symbol, "HONK", 5);
  assert(client.s.act(create, clock));
  step(16000, true);
  assert(!client.s.pending && client.s.world.coins == 1 &&
         host.s.world.coins == 1);
  assert(host.s.world.p[1].balance == 23000);
  Request purchase{};
  purchase.op = Op::Buy;
  purchase.coin = 1;
  purchase.amount = 7;
  assert(client.s.act(purchase, clock));
  step(16000, true);
  assert(!client.s.pending && owned(host.s.world.p[1], 1) == 7);
  assert(client.s.world.p[1].balance == host.s.world.p[1].balance);
  // A missing host must not turn a speculative trade into a committed one.
  auto original = client.s.world.p[1].balance;
  assert(client.s.act(purchase, clock));
  for (int i = 0; i < 1000; i++) {
    clock += 20;
    client.s.tick(clock);
    messages.clear();
  }
  assert(client.s.pending && client.s.world.p[1].balance == original);
  step(16000, true);
  assert(!client.s.pending && owned(host.s.world.p[1], 1) == 14);
  client.duel.challenge(mac(1), 919, clock);
  step(2000, true);
  assert(host.duel.phase == Duel::Invite);
  host.duel.press(true, clock);
  step(600, true);
  step(4000, true);
  if (client.duel.phase == Duel::Go)
    client.duel.press(true, clock);
  if (host.duel.phase == Duel::Go)
    host.duel.press(true, clock);
  step(5000, true);
  assert(client.duel.phase == Duel::Done && host.duel.phase == Duel::Done);
  auto phase = host.duel.phase;
  host.duel.receive(mac(2), 20, client.duel.nonce, client.duel.seed, clock);
  assert(host.duel.phase == phase);
  auto revision = host.s.world.revision;
  for (int i = 0; i < 10000; i++) {
    uint8_t junk[PacketMax];
    size_t size = random() % PacketMax;
    for (size_t j = 0; j < size; j++)
      junk[j] = random();
    host.s.receive(mac(77), -40, junk, size, clock);
  }
  assert(host.s.world.revision == revision);
  Node retry(3);
  retry.s.peers[0] = {mac(1), 99, clock, -40};
  retry.s.peerCount = 1;
  assert(retry.s.join(0, clock));
  retry.s.cancel_join();
  assert(retry.s.mode == Mode::Menu);
  printf(
      "PASS: 30,000 conserved trades, snapshots, 20,000 malformed save inputs, "
      "reaction proof, lossy two-badge trades, host outage, duel. Dropped=%d "
      "duplicated=%d. Market=%zu Session=%zu max save=%zu bytes\n",
      dropped, duplicated, sizeof(Market), sizeof(Session), n);
}
