#include "session.hpp"
#include <algorithm>
namespace bm {
static constexpr Mac All{{255, 255, 255, 255, 255, 255}};
void Session::init(Mac mac, Send fn, Entropy random, void *ctx) {
  *this = Session{};
  self = mac;
  send = fn;
  entropy = random;
  context = ctx;
}
bool Session::frame(uint8_t kind, Mac to, uint32_t serial, const uint8_t *data,
                    size_t n) {
  if (!send || n > PacketMax - 24)
    return false;
  uint8_t b[PacketMax];
  Writer w{b, sizeof b};
  w.u32(0x324d424e);
  w.u8(kind);
  w.u8(1);
  w.u32(market_);
  w.u32(serial);
  w.bytes(to.b, 6);
  w.bytes(data, n);
  w.u32(crc32(b, w.pos));
  return w.ok && send(context, b, w.pos);
}
void Session::start(bool hosting, uint64_t now, const Market *saved) {
  pending = false;
  txSize_ = 0;
  rxRevision_ = rxMask_ = 0;
  mode = hosting ? Mode::Host : Mode::Solo;
  if (saved)
    world = *saved;
  else
    world.reset(entropy(context));
  player = world.join(self);
  if (player < 0) {
    mode = Mode::Menu;
    status = "SAVED MARKET FULL";
    return;
  }
  market_ = world.id;
  host = self;
  epochAt_ = now + 120000;
  syncAt_ = now;
  status = "MARKET OPEN";
  changed = true;
}
bool Session::join(unsigned i, uint64_t now) {
  if (i >= peerCount || now - peers[i].seen > 8000)
    return false;
  host = peers[i].mac;
  market_ = peers[i].market;
  mode = Mode::Client;
  player = -1;
  lastSeen = 0;
  rxRevision_ = rxMask_ = 0;
  rxSize_ = txSize_ = 0;
  world = Market{};
  world.id = market_;
  retryAt_ = now;
  syncAt_ = now;
  pending = false;
  status = "JOINING HOST";
  changed = true;
  return true;
}
void Session::cancel_join() {
  if (mode == Mode::Client && player < 0) {
    mode = Mode::Menu;
    pending = false;
    status = "CHOOSE A MARKET";
    changed = true;
  }
}
bool Session::act(Request r, uint64_t now) {
  if (pending || player < 0) {
    status = pending ? "WAITING FOR HOST" : "JOIN A MARKET FIRST";
    changed = true;
    return false;
  }
  r.seq = world.p[player].next;
  if (authority()) {
    last = world.request(player, r, now, entropy(context));
    completed = r.seq;
    completedOp = r.op;
    status = error_text(last.code);
    changed = true;
    syncAt_ = 0;
    return true;
  }
  if (mode != Mode::Client)
    return false;
  request_ = r;
  pending = true;
  retryAt_ = now;
  status = "TRADE PENDING";
  changed = true;
  return true;
}
void Session::queue_snapshot() {
  txSize_ = encode(world, tx_, sizeof tx_);
  txOffset_ = 0;
  txRevision_ = world.revision;
}
bool Session::send_social(Mac target, uint8_t kind, uint32_t nonce,
                          uint32_t value) {
  if (kind < 20 || kind > 26 || player < 0)
    return false;
  uint8_t b[4];
  Writer w{b, 4};
  w.u32(value);
  return frame(kind, target, nonce, b, 4);
}
void Session::tick(uint64_t now) {
  if (authority() && now >= epochAt_) {
    world.advance_epoch();
    epochAt_ = now + 120000;
    changed = true;
  }
  if (mode == Mode::Host) {
    if (now >= beaconAt_) {
      frame(0, All, 0, nullptr, 0);
      beaconAt_ = now + 1700 + entropy(context) % 500;
    }
    if (!txSize_ && now >= syncAt_) {
      queue_snapshot();
      syncAt_ = now + 2500;
    }
    if (txSize_ && now >= txAt_) {
      uint8_t b[Chunk + 4];
      Writer w{b, sizeof b};
      w.u16(txSize_);
      w.u16(txOffset_);
      auto n = std::min(size_t(txSize_ - txOffset_), Chunk);
      w.bytes(tx_ + txOffset_, n);
      if (frame(3, All, txRevision_, b, w.pos)) {
        txOffset_ += n;
        if (txOffset_ == txSize_)
          txSize_ = 0;
      }
      txAt_ = now + 35;
    }
  } else if (mode == Mode::Client) {
    if (pending && now >= retryAt_) {
      uint8_t b[26];
      Writer w{b, sizeof b};
      w.u8(uint8_t(request_.op));
      w.u8(request_.coin);
      w.u16(request_.amount);
      w.bytes(request_.symbol, 6);
      w.bytes(request_.proof, 16);
      frame(2, host, request_.seq, b, w.pos);
      retryAt_ = now + 850 + entropy(context) % 250;
    }
    if (now >= syncAt_) {
      frame(1, host, 0, nullptr, 0);
      syncAt_ = now + 2000 + entropy(context) % 700;
    }
    if (now > lastSeen + 8000 && lastSeen) {
      status = pending ? "HOST LOST - RETRYING" : "HOST OUT OF RANGE";
      changed = true;
    }
  }
}
void Session::receive(Mac from, int8_t rssi, const uint8_t *b, size_t n,
                      uint64_t now) {
  if (from == self)
    return;
  if (n < 24 || n > PacketMax) {
    malformed++;
    return;
  }
  Reader tail{b + n - 4, 4};
  if (crc32(b, n - 4) != tail.u32()) {
    malformed++;
    return;
  }
  Reader r{b, n - 4};
  if (r.u32() != 0x324d424e)
    return;
  uint8_t kind = r.u8();
  if (r.u8() != 1)
    return;
  uint32_t market = r.u32(), serial = r.u32();
  Mac target;
  r.bytes(target.b, 6);
  if (target != self && target != All)
    return;
  size_t payload = r.size - r.pos;
  if (kind == 0 && payload == 0 && mode == Mode::Menu) {
    unsigned i = 0;
    for (; i < peerCount; i++)
      if (peers[i].mac == from)
        break;
    if (i == peerCount) {
      if (peerCount < 8)
        peerCount++;
      else {
        i = 0;
        for (unsigned j = 1; j < 8; j++)
          if (peers[j].seen < peers[i].seen)
            i = j;
      }
    }
    peers[i] = {from, market, now, rssi};
    changed = true;
    return;
  }
  if (market != market_)
    return;
  if (kind >= 20 && kind <= 26 && payload == 4 && player >= 0 &&
      world.find(from) >= 0) {
    auto value = r.u32();
    if (social)
      social(socialContext, from, kind, serial, value, now);
    return;
  }
  if (mode == Mode::Host && target == self) {
    if (kind == 1 && payload == 0) {
      int p = world.join(from);
      if (p < 0) {
        uint8_t error = uint8_t(Error::Full);
        frame(4, from, 0, &error, 1);
      } else
        syncAt_ = 0;
      changed = true;
    } else if (kind == 2 && payload == 26) {
      int p = world.find(from);
      if (p < 0)
        return;
      Request q{};
      q.seq = serial;
      q.op = Op(r.u8());
      q.coin = r.u8();
      q.amount = r.u16();
      r.bytes(q.symbol, 6);
      r.bytes(q.proof, 16);
      auto result = world.request(p, q, now, entropy(context));
      uint8_t ack[6];
      Writer w{ack, 6};
      w.u8(uint8_t(result.code));
      w.u8(result.coin);
      w.u32(result.ticket);
      frame(5, from, serial, ack, w.pos);
      syncAt_ = 0;
      changed = true;
    }
    return;
  }
  if (mode != Mode::Client || from != host)
    return;
  if (kind == 4 && payload == 1) {
    status = error_text(Error(r.u8()));
    changed = true;
    return;
  }
  // State snapshots carry the committed request result, so a lost ACK is
  // harmless.
  if (kind != 3 || payload < 5)
    return;
  uint16_t total = r.u16(), offset = r.u16();
  size_t count = r.size - r.pos;
  if (total < 20 || total > MaxSave || offset >= total || offset % Chunk ||
      count != std::min(Chunk, size_t(total - offset)) ||
      serial < world.revision)
    return;
  if (serial != rxRevision_ || total != rxSize_) {
    if (serial < rxRevision_)
      return;
    rxRevision_ = serial;
    rxSize_ = total;
    rxMask_ = 0;
  }
  std::memcpy(rx_ + offset, b + r.pos, count);
  rxMask_ |= 1u << (offset / Chunk);
  unsigned pieces = (total + Chunk - 1) / Chunk;
  if (rxMask_ != ((1u << pieces) - 1))
    return;
  Market incoming;
  if (!decode(incoming, rx_, total) || incoming.id != market_ ||
      incoming.revision != serial) {
    malformed++;
    rxMask_ = 0;
    return;
  }
  int id = incoming.find(self);
  if (id < 0)
    return;
  world = incoming;
  player = id;
  lastSeen = now;
  changed = true;
  if (pending && world.p[player].lastSeq == request_.seq &&
      world.p[player].lastHash == request_hash(request_)) {
    last = world.p[player].last;
    completed = request_.seq;
    completedOp = request_.op;
    pending = false;
    status = error_text(last.code);
  } else if (pending && world.p[player].next > request_.seq) {
    pending = false;
    status = "REQUEST SUPERSEDED";
  } else if (!pending)
    status = "MARKET SYNCED";
}
} // namespace bm
