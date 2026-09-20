#include "games.hpp"
#include <algorithm>
namespace bm {
void Reaction::start(uint32_t s, uint64_t now) {
  *this = Reaction{};
  seed = s;
  phase = Pause;
  deadline = now + 800;
}
bool Reaction::tick(uint64_t now) {
  if (phase == Pause && now >= deadline) {
    if (++round > 8) {
      phase = Done;
      return true;
    }
    unsigned delay;
    key = command(seed, round, delay);
    phase = Wait;
    deadline = now + delay;
    return true;
  }
  if (phase == Wait && now >= deadline) {
    phase = Go;
    at = now;
    return true;
  }
  if (phase == Go && now - at >= 1800) {
    press(3, now);
    return true;
  }
  return false;
}
void Reaction::press(uint8_t k, uint64_t now) {
  if ((phase != Go && phase != Wait) || !round || round > 8)
    return;
  unsigned dt = phase == Go ? std::min(uint64_t(1800), now - at) : 100;
  if (phase == Wait || dt < 100)
    k = 3;
  proof[round - 1] = k;
  proof[round + 7] = std::max(5u, (dt + 19) / 20);
  if (k == key)
    score++;
  phase = Pause;
  deadline = now + 180;
}
void Duel::init(Session &s) {
  session = &s;
  s.socialContext = this;
  s.social = [](void *p, Mac m, uint8_t k, uint32_t n, uint32_t v, uint64_t t) {
    static_cast<Duel *>(p)->receive(m, k, n, v, t);
  };
}
void Duel::queue(uint8_t k, uint32_t v, uint64_t now) {
  packet = k;
  value = v;
  sendAt = now;
}
void Duel::challenge(Mac p, uint32_t random, uint64_t now) {
  peer = p;
  nonce = random ? random : 1;
  seed = nonce;
  haveMine = haveTheirs = false;
  phase = Offer;
  limit = now + 15000;
  queue(20, seed, now);
  changed = true;
}
void Duel::receive(Mac from, uint8_t k, uint32_t n, uint32_t v, uint64_t now) {
  if (k == 20 && from == peer && n == nonce &&
      (phase == Done || phase == Cancelled))
    return;
  if (k == 20 && (phase == Idle || phase == Done || phase == Cancelled)) {
    peer = from;
    nonce = n;
    seed = v;
    phase = Invite;
    limit = now + 15000;
    packet = 0;
    haveMine = haveTheirs = false;
    changed = true;
    return;
  }
  if (from != peer || n != nonce)
    return;
  if (k == 21 && phase == Offer) {
    phase = Wait;
    at = now + 1500 + seed % 2000;
    limit = at + 12000;
    queue(22, 0, now);
  } else if (k == 22) {
    if (phase == Accept) {
      phase = Wait;
      at = now + 1500 + seed % 2000;
      limit = at + 12000;
      packet = 0;
    }
    session->send_social(peer, 23, nonce, 0);
  } else if (k == 23 && packet == 22)
    packet = 0;
  else if (k == 24 &&
           (phase == Wait || phase == Go || phase == Result || phase == Done)) {
    if (v > 9999 && v != 65535)
      return;
    theirs = v;
    haveTheirs = true;
    session->send_social(peer, 25, nonce, 0);
    if (haveMine)
      phase = Done;
  } else if (k == 25 && packet == 24)
    packet = 0;
  else if (k == 26) {
    phase = Cancelled;
    packet = 0;
  }
  changed = true;
}
void Duel::press(bool accept, uint64_t now) {
  if (!accept) {
    session->send_social(peer, 26, nonce, 0);
    phase = Idle;
    packet = 0;
  } else if (phase == Invite) {
    phase = Accept;
    queue(21, 0, now);
  } else if (phase == Wait || phase == Go) {
    uint64_t dt = now >= at ? now - at : 0;
    mine = phase == Go && dt >= 100 && dt < 5000 ? dt : 65535;
    haveMine = true;
    phase = haveTheirs ? Done : Result;
    queue(24, mine, now);
  }
  changed = true;
}
void Duel::tick(uint64_t now) {
  if (phase == Idle)
    return;
  if (packet && now >= sendAt) {
    session->send_social(peer, packet, nonce, value);
    sendAt = now + 650;
  }
  if (phase == Wait && now >= at) {
    phase = Go;
    changed = true;
  }
  if (phase == Go && now >= at + 5000)
    press(true, now);
  if (now >= limit) {
    packet = 0;
    if (phase != Done && phase != Cancelled) {
      phase = Cancelled;
      changed = true;
    }
  }
}
} // namespace bm
