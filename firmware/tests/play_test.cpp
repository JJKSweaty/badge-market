#include "bomb.hpp"
#include "play.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <random>
#include <vector>
using namespace bm;
static Mac mac(int n) { return Mac{{2, 0, 0, 0, 0, uint8_t(n)}}; }
struct Packet {
  int from;
  std::vector<uint8_t> bytes;
};
static std::vector<Packet> packets;
struct Node {
  Session session;
  Bomb bomb;
  int id;
  uint32_t seed = 733;
  Node(int n) : id(n) {
    session.init(
        mac(n),
        [](void *p, const uint8_t *b, size_t n) {
          auto &a = *static_cast<Node *>(p);
          packets.push_back({a.id, {b, b + n}});
          return true;
        },
        [](void *p) { return random_step(static_cast<Node *>(p)->seed); },
        this);
  }
};
int main() {
  uint8_t marks[5];
  evaluate_word("APPLE", "PUPPY", marks);
  assert(marks[0] == 1 && marks[1] == 0 && marks[2] == 2 && marks[3] == 0 &&
         marks[4] == 0);
  evaluate_word("EERIE", "GEESE", marks);
  assert(marks[1] == 2 && marks[2] == 1 && marks[4] == 2);
  for (unsigned i = 0; i < answer_count(); i++) {
    char word[6];
    answer_word(i, word);
    assert(valid_word(word));
  }
  assert(!valid_word("ZZZZZ"));
  std::mt19937 rng(99);
  for (unsigned seed = 1; seed <= 2000; seed++) {
    Runner run;
    run.start(seed);
    while (!run.done) {
      int nearest = -1000;
      unsigned blocked = 0;
      for (auto &o : run.objects)
        if (o.active && o.kind <= 2 && o.y > nearest && o.y < 199)
          nearest = o.y;
      for (auto &o : run.objects)
        if (o.active && o.kind <= 2 && o.y == nearest)
          blocked |= 1 << o.lane;
      if (nearest > 80 && nearest < 175) {
        int safe = 0;
        while (safe < 3 && (blocked & (1 << safe)))
          ++safe;
        if (safe < 3 && safe != run.lane)
          run.input(safe < run.lane ? 4 : 5);
      }
      run.step();
    }
    assert(run.hearts == 3 && run.tick == Runner::Duration && !run.overflow);
    assert(Runner::verify(seed, run.trace, run.traceCount * 3, run.tick,
                          run.score));
    assert(!Runner::verify(seed, run.trace, run.traceCount * 3, run.tick,
                           run.score + 1));
  }
  for (int players : {2, 3, 6}) {
    std::vector<std::unique_ptr<Node>> nodes;
    for (int i = 0; i < players; i++)
      nodes.emplace_back(new Node(i + 1));
    uint64_t now = 0;
    int dropped = 0;
    auto step = [&](unsigned ms, bool loss = true) {
      for (unsigned t = 0; t < ms; t += 20) {
        now += 20;
        for (auto &n : nodes) {
          n->session.tick(now);
          n->bomb.tick(now);
        }
        auto work = std::move(packets);
        packets.clear();
        if (rng() % 2)
          std::reverse(work.begin(), work.end());
        for (auto &msg : work)
          for (auto &n : nodes)
            if (n->id != msg.from) {
              if (loss && rng() % 8 == 0) {
                dropped++;
                continue;
              }
              n->session.receive(mac(msg.from), -40, msg.bytes.data(),
                                 msg.bytes.size(), now);
              if (loss && rng() % 4 == 0)
                n->session.receive(mac(msg.from), -40, msg.bytes.data(),
                                   msg.bytes.size(), now);
            }
      }
    };
    auto &host = *nodes[0];
    host.session.start(true, 0);
    step(4000);
    for (int i = 1; i < players; i++) {
      assert(nodes[i]->session.peerCount);
      nodes[i]->session.join(0, now);
    }
    step(15000);
    for (auto &n : nodes) {
      assert(n->session.player >= 0);
      n->bomb.enter(n->session, now);
    }
    host.bomb.create(now);
    step(2000);
    for (int i = 1; i < players; i++)
      nodes[i]->bomb.input(0, now);
    step(2500);
    assert(host.bomb.count == players);
    for (int i = 1; i < players; i++) {
      assert(nodes[i]->bomb.member(nodes[i]->session.player) >= 0);
      nodes[i]->bomb.input(0, now);
    }
    host.bomb.input(0, now);
    step(4500);
    assert(host.bomb.phase == Bomb::Live);
    for (auto &n : nodes)
      assert(n->bomb.phase == Bomb::Live);
    auto from = host.bomb.holder;
    int receiver = (from + 1) % players;
    auto byPlayer = [&](int p) -> Node & {
      for (auto &n : nodes)
        if (n->session.player == p)
          return *n;
      std::abort();
    };
    auto &sender = byPlayer(from);
    sender.bomb.input(0, now);
    sender.bomb.selection = sender.bomb.member(receiver);
    sender.bomb.input(0, now);
    step(300, false);
    assert(host.bomb.target == receiver);
    auto &dest = byPlayer(receiver);
    assert(dest.bomb.target == receiver);
    dest.bomb.input(dest.bomb.key, now);
    step(400, false);
    assert(host.bomb.holder == receiver && host.bomb.target == 255);
    // Same challenge response delivered again cannot transfer again or add
    // bonuses.
    auto passed = host.bomb.members[host.bomb.member(from)].passes;
    uint8_t proof[5];
    Writer wr{proof, 5};
    wr.u32(host.bomb.challenge);
    wr.u8(host.bomb.key);
    host.bomb.process(receiver, 46, proof, 5, now);
    assert(host.bomb.members[host.bomb.member(from)].passes == passed);
    uint8_t returnTarget = from;
    host.bomb.process(receiver, 44, &returnTarget, 1, now);
    assert(host.bomb.target == 255); // Immediate pass-back is host-rejected.
    step(2200, false);
    host.bomb.process(receiver, 44, &returnTarget, 1, now);
    step(250, false);
    assert(host.bomb.target == from);
    auto &wrong = byPlayer(from);
    wrong.bomb.input(wrong.bomb.key == 0 ? 1 : 0, now);
    step(450, false);
    assert(host.bomb.target == 255 && host.bomb.holder == receiver);
    host.bomb.process(receiver, 44, &returnTarget, 1, now);
    step(2200, false);
    assert(host.bomb.target == 255 &&
           host.bomb.holder == receiver); // Missed challenge.
    step(26000);
    assert(host.bomb.phase == Bomb::Over);
    for (auto &n : nodes)
      assert(n->bomb.phase == Bomb::Over && n->bomb.holder == host.bomb.holder);
    uint32_t balances[6];
    for (int i = 0; i < players; i++) {
      balances[i] = host.session.world.p[i].balance;
      assert(balances[i] > 25000);
    }
    step(3000);
    for (int i = 0; i < players; i++)
      assert(host.session.world.p[i].balance == balances[i]);
    for (auto &n : nodes)
      n->bomb.exit(now);
    printf("PASS: %d Bomb players, interactive pass, canonical explosion, "
           "duplicate settlement suppression, %d lost deliveries\n",
           players, dropped);
    packets.clear();
  }
  puts("PASS: flash dictionary, duplicate letters, 2,000 fair seeded Run "
       "routes and replay proofs.");
}
