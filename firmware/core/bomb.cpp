#include "bomb.hpp"
#include <algorithm>
#include <climits>
namespace bm {
static constexpr Mac All{{255, 255, 255, 255, 255, 255}};
int Bomb::member(unsigned p) const {
  for (int i = 0; i < count; i++)
    if (members[i].player == p)
      return i;
  return -1;
}
bool Bomb::mine() const { return session && holder == session->player; }
void Bomb::enter(Session &s, uint64_t now) {
  *this = Bomb{};
  session = &s;
  host = s.mode == Mode::Host;
  seenAt = now;
  message = host                   ? "A CREATE GAME"
            : s.mode == Mode::Solo ? "HOST A MARKET TO PLAY"
                                   : "WAITING FOR HOST LOBBY";
  s.gameContext = this;
  s.gameReceive = [](void *p, Mac m, uint8_t k, uint32_t q, const uint8_t *b,
                     size_t n, uint64_t t) {
    static_cast<Bomb *>(p)->receive(m, k, q, b, n, t);
  };
}
void Bomb::exit(uint64_t now) {
  if (!session)
    return;
  if (host && phase != Off && phase != Over) {
    phase = Cancelled;
    announce(now);
    send_state();
  } else if (!host && id) {
    uint8_t b[8];
    Writer w{b, 8};
    w.u32(id);
    w.u32(round);
    session->send_game(session->host, 52, ++cmdSeq, b, 8);
  }
  session->gameReceive = nullptr;
  session->gameContext = nullptr;
  session = nullptr;
}
void Bomb::create(uint64_t now) {
  if (!host)
    return;
  id = session->entropy(session->context);
  if (!id)
    id = 1;
  privateSeed = session->entropy(session->context) ^ 0xb76241;
  count = 1;
  members[0] = {};
  members[0].player = session->player;
  members[0].seen = now;
  phase = Lobby;
  holder = target = 255;
  message = "A READY / HOST STARTS WHEN ALL READY";
  announce(now);
}
void Bomb::announce(uint64_t now) {
  ++revision;
  sendAt = now;
  phaseAt = now;
  retryUntil = now + 8000;
  changed = true;
}
void Bomb::send_state() {
  uint8_t b[100];
  Writer w{b, sizeof b};
  w.u32(id);
  w.u32(round);
  w.u8(phase);
  w.u8(count);
  w.u8(holder);
  w.u8(previous);
  w.u8(target);
  w.u8(key);
  w.u8(tension);
  w.u32(challenge);
  for (int i = 0; i < count; i++) {
    auto &m = members[i];
    w.u8(m.player);
    w.u8(m.ready);
    w.u8(m.left);
    w.u8(m.passes);
    w.u16(m.reward);
    w.u32(m.command);
  }
  uint8_t kind = phase == Lobby       ? 40
                 : phase == Countdown ? 43
                 : phase == Over      ? 49
                 : target != 255      ? 45
                                      : 47;
  session->send_game(All, kind, revision, b, w.pos);
}
void Bomb::command(uint8_t kind, const uint8_t *b, size_t n, uint64_t now) {
  if (host) {
    process(session->player, kind, b, n, now);
    return;
  }
  if (commandKind)
    return;
  commandKind = kind;
  ++cmdSeq;
  commandSize = n + 8;
  Writer w{commandData, sizeof commandData};
  w.u32(id);
  w.u32(round);
  w.bytes(b, n);
  commandAt = now;
}
void Bomb::process(unsigned pid, uint8_t kind, const uint8_t *b, size_t n,
                   uint64_t now) {
  int idx = member(pid);
  if (kind == 41 && phase == Lobby && idx < 0 && count < 6) {
    idx = count++;
    members[idx] = {};
    members[idx].player = pid;
    members[idx].seen = now;
    announce(now);
  }
  if (idx < 0)
    return;
  auto &m = members[idx];
  m.seen = now;
  if (kind == 42 && phase == Lobby && n == 1) {
    m.ready = b[0] != 0;
    announce(now);
  }
  if (kind == 52) {
    m.left = true;
    m.ready = false;
    announce(now);
  }
  if (phase != Live || now >= deadline)
    return;
  if (kind == 44 && n == 1 && pid == holder && target == 255 && !m.left) {
    int dest = member(b[0]);
    if (dest < 0 || members[dest].left || b[0] == holder ||
        (b[0] == previous && now < lockUntil))
      return;
    target = b[0];
    key = uint8_t(random_step(privateSeed) % 6);
    static constexpr uint8_t keys[] = {0, 1, 6, 3, 4, 5};
    key = keys[key];
    ++challenge;
    challengeLimit = now + 2000;
    challengeAck = false;
    responded = false;
    message = "PASS PENDING";
    announce(now);
  }
  if (kind == 46 && n == 5 && pid == target) {
    Reader r{b, n};
    auto c = r.u32();
    auto pressed = r.u8();
    if (c != challenge || now >= challengeLimit)
      return;
    if (pressed == key) {
      int from = member(holder);
      if (from >= 0)
        members[from].passes = std::min(5, int(members[from].passes) + 1);
      previous = holder;
      holder = target;
      lockUntil = now + 2000;
      message = "RUG PASSED";
    } else
      message = "PASS FAILED";
    target = 255;
    announce(now);
  }
}
void Bomb::input(int k, uint64_t now) {
  if (phase == Off) {
    if (k == 0 && host)
      create(now);
    else if (k == 0 && id)
      command(41, nullptr, 0, now);
    return;
  }
  int me = member(session->player);
  if (phase == Lobby) {
    if (k == 0) {
      if (me < 0)
        command(41, nullptr, 0, now);
      else {
        uint8_t ready = !members[me].ready;
        command(42, &ready, 1, now);
      }
    }
    return;
  }
  if (phase != Live)
    return;
  if (target == session->player && !responded) {
    if (now >= challengeLimit)
      return;
    if (k != 0 && k != 1 && k != 3 && k != 4 && k != 5 && k != 6)
      return;
    uint8_t data[5];
    Writer w{data, 5};
    w.u32(challenge);
    w.u8(k);
    command(46, data, 5, now);
    responded = true;
    return;
  }
  if (!mine() || target != 255)
    return;
  if (k == 1) {
    selecting = false;
    return;
  }
  if (k == 0 && !selecting) {
    selecting = true;
    selection = 0;
    while (selection < count && members[selection].player == holder)
      ++selection;
    return;
  }
  if (selecting) {
    if (k == 6)
      selection = (selection + count - 1) % count;
    if (k == 3)
      selection = (selection + 1) % count;
    if (k == 0 && selection < count) {
      uint8_t p = members[selection].player;
      if (p == holder || members[selection].left) {
        message = "CHOOSE ANOTHER CONNECTED BADGE";
        changed = true;
        return;
      }
      if (p == previous && now < lockUntil) {
        message = "RETURN PASS LOCKED - WAIT 2S";
        selecting = false;
        changed = true;
        return;
      }
      message = "PASS PENDING";
      command(44, &p, 1, now);
      selecting = false;
    }
  }
}
void Bomb::tick(uint64_t now) {
  if (!session || (phase == Off && !id))
    return;
  if (host) {
    int me = member(session->player);
    if (me >= 0)
      members[me].seen = now;
    if (phase == Lobby) {
      // Remove stale/left lobby slots so retries cannot fill a lobby forever.
      for (int i = count - 1; i >= 0; i--)
        if (members[i].left || now > members[i].seen + 8000) {
          for (int j = i; j + 1 < count; j++)
            members[j] = members[j + 1];
          --count;
          announce(now);
        }
      bool ready = count >= 2;
      for (int i = 0; i < count; i++)
        ready &= members[i].ready;
      if (ready && session->world.bombSerial < UINT32_MAX) {
        round = ++session->world.bombSerial;
        session->world.dirty = true;
        ++session->world.revision;
        phase = Countdown;
        deadline = now + 3000;
        holder = members[random_step(privateSeed) % count].player;
        announce(now);
      }
    } else if (phase == Countdown && now >= deadline) {
      bool allAck = true;
      for (int i = 0; i < count; i++)
        if (members[i].player != session->player && members[i].ack < revision)
          allAck = false;
      if (!allAck) {
        phase = Cancelled;
        message = "START NOT RECEIVED - TRY AGAIN";
        announce(now);
        return;
      }
      phase = Live;
      deadline = now + 10000 + random_step(privateSeed) % 15001;
      message = "PASS IT!";
      announce(now);
    } else if (phase == Live) {
      unsigned alive = 0;
      for (int i = 0; i < count; i++) {
        if (now > members[i].seen + 5000)
          members[i].left = true;
        if (!members[i].left)
          ++alive;
      }
      if (alive < 2) {
        phase = Cancelled;
        message = "PLAYERS DISCONNECTED";
        announce(now);
      } else if (now >= deadline) {
        phase = Over;
        target = 255;
        for (int i = 0; i < count; i++) {
          auto &m = members[i];
          m.reward = session->world.bomb_reward(m.player, round,
                                                m.left ? 0
                                                : m.player == holder
                                                    ? 250
                                                    : 2000 + 100 * m.passes);
        }
        message = "ROUND COMPLETE";
        announce(now);
      } else {
        uint8_t heat = deadline - now < 3500   ? 2
                       : deadline - now < 8000 ? 1
                                               : 0;
        if (heat != tension) {
          tension = heat;
          announce(now);
        }
        if (target != 255 && now >= challengeLimit) {
          target = 255;
          message = "PASS FAILED";
          announce(now);
        }
      }
    }
    bool missing = phase == Lobby;
    for (int i = 0; i < count; i++)
      if (members[i].player != session->player && !members[i].left &&
          members[i].ack < revision)
        missing = true;
    if (missing && now >= sendAt && (phase == Lobby || now < retryUntil)) {
      send_state();
      sendAt = now + (phase == Lobby ? 900 : 250);
    }
  } else {
    if (now > seenAt + 8000) {
      phase = Cancelled;
      message = "HOST LOST - RETURN TO GAMES";
      commandKind = 0;
      changed = true;
    }
    if (ackAt && now >= ackAt) {
      uint8_t b[12];
      Writer w{b, 12};
      w.u32(id);
      w.u32(round);
      w.u32(revision);
      session->send_game(session->host, 50, revision, b, 12);
      ackAt = 0;
    }
    if (commandKind && now >= commandAt) {
      session->send_game(session->host, commandKind, cmdSeq, commandData,
                         commandSize);
      commandAt = now + 400;
    }
    if (now >= sendAt) {
      uint8_t b[12];
      Writer w{b, 12};
      w.u32(id);
      w.u32(round);
      w.u32(revision);
      session->send_game(session->host, 53, revision, b, 12);
      sendAt = now + 1400;
    }
  }
}
void Bomb::receive(Mac from, uint8_t kind, uint32_t seq, const uint8_t *b,
                   size_t n, uint64_t now) {
  int pid = session->world.find(from);
  if (pid < 0)
    return;
  if (host) {
    if (n < 8)
      return;
    Reader r{b, n};
    if (r.u32() != id || r.u32() != round)
      return;
    int idx = member(pid);
    if ((kind == 50 || kind == 53) && n == 12 && idx >= 0) {
      members[idx].seen = now;
      members[idx].ack = std::min(r.u32(), revision);
      if (kind == 50 && pid == target && !challengeAck &&
          members[idx].ack == revision) {
        challengeAck = true;
        challengeLimit = std::min(challengeLimit, now + 1400);
      }
      // Liveness reply carries state only if recovery is needed; otherwise ACK.
      if (kind == 53) {
        uint8_t data[4];
        Writer w{data, 4};
        w.u32(id);
        session->send_game(from, 50, revision, data, 4);
        if (members[idx].ack < revision) {
          sendAt = now;
          retryUntil = now + 2000;
        }
      }
      return;
    }
    if (kind != 41 && kind != 42 && kind != 44 && kind != 46 && kind != 52)
      return;
    if (idx >= 0 && seq <= members[idx].command) {
      sendAt = now;
      return;
    }
    if (phase == Live && now >= deadline) {
      tick(now);
      return;
    }
    process(pid, kind, b + 8, n - 8, now);
    idx = member(pid);
    if (idx >= 0) {
      members[idx].command = seq;
      announce(now);
    }
    return;
  }
  if (from != session->host)
    return;
  if (kind == 50 && n == 4) {
    Reader r{b, n};
    if (r.u32() == id)
      seenAt = now;
    return;
  }
  if (kind != 40 && kind != 43 && kind != 45 && kind != 47 && kind != 49)
    return;
  if (n < 19)
    return;
  Reader r{b, n};
  auto incoming = r.u32();
  auto gameRound = r.u32();
  auto ph = r.u8(), cnt = r.u8();
  if (!incoming || ph > Cancelled || cnt > 6 || n != 19 + size_t(cnt) * 10 ||
      (incoming == id && seq < revision))
    return;
  auto own = r.u8(), prev = r.u8(), dest = r.u8(), button = r.u8(),
       heat = r.u8();
  auto ch = r.u32();
  Member list[6]{};
  uint16_t seen = 0;
  for (int i = 0; i < cnt; i++) {
    auto &m = list[i];
    m.player = r.u8();
    m.ready = r.u8();
    m.left = r.u8();
    m.passes = r.u8();
    m.reward = r.u16();
    m.command = r.u32();
    if (m.player >= session->world.players || seen & (1 << m.player) ||
        m.passes > 5 || m.reward > 2500)
      return;
    seen |= 1 << m.player;
  }
  if (ph >= Countdown && ph <= Over &&
      (own >= session->world.players || !(seen & (1 << own))))
    return;
  bool newChallenge = ch != challenge || dest != target;
  bool passed = phase == Live && own != holder;
  bool failed = phase == Live && target != 255 && dest == 255 && own == holder;
  bool updated = incoming != id || seq > revision;
  if (passed)
    lockUntil = now + 2000;
  if (incoming != id) {
    cmdSeq = 0;
    commandKind = 0;
  }
  if (phase != Phase(ph))
    phaseAt = now;
  id = incoming;
  round = gameRound;
  phase = Phase(ph);
  count = cnt;
  holder = own;
  previous = prev;
  target = dest;
  key = button;
  tension = heat;
  challenge = ch;
  std::copy_n(list, 6, members);
  revision = seq;
  seenAt = now;
  ackAt = now + 20 + (session->player % 6) * 25;
  int me = member(session->player);
  if (me >= 0 && members[me].command >= cmdSeq)
    commandKind = 0;
  if (newChallenge) {
    responded = false;
    challengeLimit = now + 1250;
  }
  if (updated)
    message = phase == Lobby  ? (me < 0 ? "A JOIN LOBBY" : "A TOGGLE READY")
              : phase == Live ? (passed        ? "RUG PASSED"
                                 : failed      ? "PASS FAILED"
                                 : dest != 255 ? "PASS PENDING"
                                               : "A PASS THE RUG")
                              : "ROUND COMPLETE";
  changed = true;
}
} // namespace bm
