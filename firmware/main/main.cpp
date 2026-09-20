#include "board.hpp"
#include "driver/usb_serial_jtag.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "feedback.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "games.hpp"
#include "radio.hpp"
#include "storage.hpp"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
using namespace bm;
namespace {
Session session;
Reaction reaction;
Duel duel;
Feedback feedback;
uint32_t ledCompleted = 0;
Duel::Phase ledDuelPhase = Duel::Idle;
enum class Scene {
  Boot,
  Scan,
  Hub,
  Market,
  Coin,
  Quantity,
  Create,
  Portfolio,
  Creator,
  Rug,
  Games,
  Reaction,
  Maze,
  Nearby,
  Duel,
  Profile,
  Leave
};
Scene scene = Scene::Boot;
Scene leaveReturn = Scene::Hub, coinReturn = Scene::Market,
      createReturn = Scene::Hub;
int leaveSelected = 0, coinSelected = 0, hubSelected = 0;
int selected = 0, coinId = 1, amount = 1, mazeX = 1, mazeY = 1;
char symbol[6] = "GOOSE";
const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
uint16_t held = 0, injected = 0;
uint64_t pulseEnd = 0, rugAt = 0, paintAt = 0, saveAt = 0, ledAt = 0,
         mazeAt = 0;
bool dirty = true, ledEnabled = true, dim = true, wantTicket = false,
     claimSent = false, mazeWon = false, saveOk = true;
const char *saveStatus = "NO CHECKPOINT YET";
unsigned rugsSeen = 0;
bool rugArmed = false;
bool editingLetter = false;
bool leaveEditing = false;
char savedLetter = 0;
void move_selection(int key, int count) {
  if (count < 1) {
    selected = 0;
    return;
  }
  if (key == board::Down)
    selected = (selected + 1) % count;
  if (key == board::Up)
    selected = (selected + count - 1) % count;
}
void go(Scene s, int selection = 0) {
  if (s == Scene::Boot) {
    session.cancel_join();
    if (radio::disable() != ESP_OK)
      session.status = "RADIO STOP FAILED";
    session.peerCount = 0;
  }
  feedback.trigger(Glow::Navigate, board::ms());
  editingLetter = false;
  scene = s;
  selected = selection;
  rugAt = 0;
  rugArmed = false;
  dirty = true;
}
void format(char *out, size_t n, const char *pattern, ...) {
  va_list a;
  va_start(a, pattern);
  vsnprintf(out, n, pattern, a);
  va_end(a);
}
template <size_t N> void str(char (&out)[N], const char *in) {
  snprintf(out, N, "%s", in);
}
void checkpoint() {
  if (session.authority()) {
    saveOk = storage::save(session.mode == Mode::Host, session.world);
    saveStatus = saveOk ? "SAVED TO FLASH" : "SAVE FAILED";
  } else
    saveStatus = "SAVED BY HOST";
  feedback.trigger(saveOk ? Glow::Save : Glow::Error, board::ms());
  dirty = true;
}
void ask_leave() {
  if (scene == Scene::Leave)
    return;
  leaveReturn = scene;
  leaveSelected = selected;
  leaveEditing = editingLetter;
  go(Scene::Leave);
}
void resume_market() {
  go(leaveReturn, leaveSelected);
  editingLetter = leaveEditing;
}
void leave_market() {
  if (session.authority()) {
    checkpoint();
    if (!saveOk) {
      session.status = "SAVE FAILED - TRY AGAIN";
      return;
    }
  }
  if (radio::disable() != ESP_OK) {
    session.status = "RADIO STOP FAILED - TRY AGAIN";
    return;
  }
  // Reset pending requests, discovery and minigames together. No old market
  // packets/reward callbacks may mutate the next session.
  auto mac = session.self;
  session.init(mac, radio::send, [](void *) { return esp_random(); }, nullptr);
  duel = Duel{};
  duel.init(session);
  reaction = Reaction{};
  wantTicket = claimSent = false;
  feedback = Feedback{};
  ledCompleted = 0;
  ledDuelPhase = Duel::Idle;
  rugsSeen = 0;
  hubSelected = 0;
  go(Scene::Boot);
}
void action(Op op, unsigned coin = 0, unsigned count = 0) {
  Request r{};
  r.op = op;
  r.coin = coin;
  r.amount = count;
  if (op == Op::Create)
    std::memcpy(r.symbol, symbol, 6);
  session.act(r, board::ms());
  dirty = true;
}
int nearby(int nth) {
  for (unsigned i = 0; i < session.world.players; i++)
    if (int(i) != session.player && nth-- == 0)
      return i;
  return -1;
}
void start(bool hosting, uint64_t now) {
  if ((hosting ? radio::enable() : radio::disable()) != ESP_OK) {
    session.status = "RADIO ERROR - TRY AGAIN";
    dirty = true;
    return;
  }
  ESP_LOGI("market", "start mode=%s heap=%lu stack_free=%u",
           hosting ? "host" : "solo", (unsigned long)esp_get_free_heap_size(),
           unsigned(uxTaskGetStackHighWaterMark(nullptr)));
  Market restored;
  bool found = storage::load(hosting, restored);
  ESP_LOGI("market", "checkpoint read complete: restored=%d", found);
  session.start(hosting, now, found ? &restored : nullptr);
  ESP_LOGI("market", "authority ready: player=%d", session.player);
  if (session.player >= 0) {
    saveStatus = found ? "CHECKPOINT RESTORED" : "NEW MARKET";
    saveAt = now + 30000;
    hubSelected = 0;
    go(Scene::Hub);
  } else
    radio::disable();
}
void tick_rug(uint64_t now) {
  if (scene != Scene::Rug)
    return;
  if (!(held & (1 << board::A))) {
    rugArmed = true;
    if (rugAt) {
      rugAt = 0;
      dirty = true;
    }
  } else if (rugArmed) {
    if (!rugAt)
      rugAt = now;
    if (now - rugAt >= 3000) {
      action(Op::Withdraw, coinId, 100);
      go(Scene::Coin);
    } else if (now >= paintAt)
      dirty = true;
  }
}
const char *reset_problem() {
  switch (esp_reset_reason()) {
  case ESP_RST_PANIC:
    return "LAST RESET: FIRMWARE PANIC";
  case ESP_RST_INT_WDT:
  case ESP_RST_TASK_WDT:
  case ESP_RST_WDT:
    return "LAST RESET: WATCHDOG";
  case ESP_RST_BROWNOUT:
    return "LAST RESET: POWER DROPPED";
  default:
    return nullptr;
  }
}
void button(int key, uint64_t now) {
  if (key == board::Up || key == board::Down || key == board::A ||
      key == board::B)
    feedback.trigger(Glow::Navigate, now);
  if (key == board::Aux) {
    ledEnabled = !ledEnabled;
    dirty = true;
    return;
  }
  if (key == board::Home) {
    if (session.player >= 0)
      ask_leave();
    else
      go(Scene::Boot);
    return;
  }
  if (scene == Scene::Leave) {
    move_selection(key, 2);
    if (key == board::B || key == board::Start ||
        (key == board::A && selected == 0))
      resume_market();
    else if (key == board::A)
      leave_market();
    dirty = true;
    return;
  }
  if (key == board::Start) {
    if (scene == Scene::Duel)
      duel.press(false, now);
    if (session.player >= 0)
      go(Scene::Hub, hubSelected);
    else
      go(Scene::Boot);
    return;
  }
  if (key == board::B && scene != Scene::Reaction && scene != Scene::Duel) {
    if (scene == Scene::Create && editingLetter) {
      symbol[selected] = savedLetter;
      editingLetter = false;
      dirty = true;
      return;
    }
    if (scene == Scene::Scan)
      go(Scene::Boot);
    else if (scene == Scene::Hub)
      ask_leave();
    else if (scene == Scene::Coin)
      go(coinReturn, coinSelected);
    else if (scene == Scene::Quantity)
      go(Scene::Coin, 2);
    else if (scene == Scene::Creator)
      go(Scene::Coin, 3);
    else if (scene == Scene::Rug)
      go(Scene::Creator, 3);
    else if (scene == Scene::Create)
      go(createReturn, createReturn == Scene::Hub ? hubSelected : 0);
    else if (scene == Scene::Maze)
      go(Scene::Games, 1);
    else if (scene != Scene::Boot && scene != Scene::Hub)
      go(Scene::Hub, hubSelected);
    return;
  }
  if (scene == Scene::Boot) {
    if (key == board::Down)
      selected = (selected + 1) % 3;
    if (key == board::Up)
      selected = (selected + 2) % 3;
    if (key == board::A) {
      if (selected < 2)
        start(selected == 1, now);
      else {
        if (radio::enable() == ESP_OK)
          go(Scene::Scan);
        else
          session.status = "RADIO ERROR - TRY AGAIN";
      }
    }
  } else if (scene == Scene::Scan) {
    if (key == board::Down && session.peerCount)
      selected = (selected + 1) % session.peerCount;
    if (key == board::Up && session.peerCount)
      selected = (selected + session.peerCount - 1) % session.peerCount;
    if (key == board::A)
      session.join(selected, now);
    if (key == board::B)
      go(Scene::Boot);
  } else if (scene == Scene::Hub) {
    move_selection(key, 6);
    if (key == board::A) {
      hubSelected = selected;
      createReturn = Scene::Hub;
      static const Scene targets[] = {Scene::Market,    Scene::Games,
                                      Scene::Portfolio, Scene::Create,
                                      Scene::Nearby,    Scene::Profile};
      go(targets[selected]);
    }
  } else if (scene == Scene::Market) {
    int n = session.world.coins;
    if (key == board::Down && n)
      selected = (selected + 1) % n;
    if (key == board::Up && n)
      selected = (selected + n - 1) % n;
    if (key == board::A) {
      if (n) {
        coinId = selected + 1;
        coinReturn = Scene::Market;
        coinSelected = selected;
        go(Scene::Coin);
      } else {
        createReturn = Scene::Market;
        go(Scene::Create);
      }
    }
  } else if (scene == Scene::Coin) {
    bool owner = session.world.c[coinId - 1].creator == session.player;
    move_selection(key, owner ? 5 : 4);
    if (key == board::A) {
      if (selected == 0)
        action(Op::Buy, coinId, amount);
      else if (selected == 1)
        action(Op::Sell, coinId, amount);
      else if (selected == 2)
        go(Scene::Quantity);
      else if (selected == 3 && owner)
        go(Scene::Creator);
      else {
        go(coinReturn, coinSelected);
      }
    }
  } else if (scene == Scene::Quantity) {
    if (key == board::Up)
      amount = std::min(100, amount + 1);
    if (key == board::Down)
      amount = std::max(1, amount - 1);
    if (key == board::A) {
      go(Scene::Coin);
      selected = 2;
    }
  } else if (scene == Scene::Create) {
    if (editingLetter) {
      if (key == board::Up || key == board::Down) {
        const char *p = std::strchr(alphabet, symbol[selected]);
        int i = p ? p - alphabet : 0, n = selected ? 36 : 26;
        symbol[selected] = alphabet[(i + (key == board::Up ? 1 : n - 1)) % n];
      }
      if (key == board::A)
        editingLetter = false;
    } else {
      move_selection(key, 6);
      if (key == board::A) {
        if (selected == 5) {
          action(Op::Create);
          if (session.pending || session.last.code == Error::Ok)
            go(Scene::Market);
        } else {
          editingLetter = true;
          savedLetter = symbol[selected];
        }
      }
    }
  } else if (scene == Scene::Portfolio) {
    move_selection(key, 4);
    if (key == board::A && session.player >= 0) {
      auto id = session.world.p[session.player].holdings[selected].coin;
      if (id) {
        coinId = id;
        coinReturn = Scene::Portfolio;
        coinSelected = selected;
        go(Scene::Coin);
      }
    }
  } else if (scene == Scene::Creator) {
    if (key == board::Down)
      selected = (selected + 1) % 4;
    if (key == board::Up)
      selected = (selected + 3) % 4;
    if (key == board::A) {
      if (selected == 3)
        go(Scene::Rug);
      else {
        static const int pct[] = {10, 25, 50};
        action(Op::Withdraw, coinId, pct[selected]);
      }
    }
  } else if (scene == Scene::Rug) {
    if (key == board::B)
      go(Scene::Coin);
  } else if (scene == Scene::Games) {
    if (key == board::Down || key == board::Up)
      selected ^= 1;
    if (key == board::A) {
      if (selected == 0) {
        Request q{};
        q.op = Op::Ticket;
        wantTicket = session.act(q, now);
      } else {
        mazeX = mazeY = 1;
        mazeWon = false;
        go(Scene::Maze);
      }
    }
  } else if (scene == Scene::Reaction) {
    if (reaction.phase == Reaction::Done && key == board::B) {
      go(Scene::Games);
      return;
    }
    auto oldPhase = reaction.phase;
    auto oldScore = reaction.score;
    if (key == board::A || key == board::B || key == board::Up)
      reaction.press(key == board::A ? 0 : key == board::B ? 1 : 2, now);
    if ((oldPhase == Reaction::Go || oldPhase == Reaction::Wait) &&
        reaction.phase == Reaction::Pause)
      feedback.trigger(reaction.score > oldScore ? Glow::Buy : Glow::Error,
                       now);
  } else if (scene == Scene::Nearby) {
    int count = std::max(0, int(session.world.players) - 1);
    if (key == board::Down && count)
      selected = (selected + 1) % count;
    if (key == board::Up && count)
      selected = (selected + count - 1) % count;
    if (key == board::A && count && session.mode != Mode::Solo) {
      int who = nearby(selected);
      if (who >= 0) {
        duel.challenge(session.world.p[who].mac, esp_random(), now);
        go(Scene::Duel);
      }
    }
  } else if (scene == Scene::Duel) {
    if (key == board::A)
      duel.press(true, now);
    if (key == board::B) {
      duel.press(false, now);
      go(Scene::Nearby);
    }
  } else if (scene == Scene::Profile) {
    move_selection(key, 4);
    if (key == board::A) {
      if (selected == 0)
        checkpoint();
      else if (selected == 1)
        ledEnabled = !ledEnabled;
      else if (selected == 2)
        dim = !dim;
      else
        go(Scene::Hub, hubSelected);
    }
  }
  dirty = true;
}
void observe_feedback(uint64_t now) {
  if (session.completed != ledCompleted) {
    ledCompleted = session.completed;
    if (session.last.code != Error::Ok)
      feedback.trigger(Glow::Error, now);
    else
      switch (session.completedOp) {
      case Op::Buy:
        feedback.trigger(Glow::Buy, now);
        break;
      case Op::Sell:
        feedback.trigger(Glow::Sell, now);
        break;
      case Op::Create:
        feedback.trigger(Glow::Launch, now);
        break;
      case Op::Claim:
      case Op::Withdraw:
        feedback.trigger(Glow::Reward, now);
        break;
      default:
        break;
      }
  }
  if (duel.phase != ledDuelPhase) {
    ledDuelPhase = duel.phase;
    if (duel.phase == Duel::Done)
      feedback.trigger(duel.mine <= duel.theirs ? Glow::Win : Glow::Error, now);
    if (duel.phase == Duel::Cancelled)
      feedback.trigger(Glow::Error, now);
  }
}
Light gameplay_light(uint64_t now) {
  Light f;
  // Immediate gameplay cues override decorative and transaction effects.
  if (scene == Scene::Rug)
    f = {uint8_t(now % 900 < 450 ? 12 : 2), 0, 0, -1};
  else if (scene == Scene::Reaction && reaction.phase == Reaction::Go)
    f = {uint8_t(reaction.key == 1 ? 12 : 0),
         uint8_t(reaction.key == 0 ? 12 : 0),
         uint8_t(reaction.key == 2 ? 12 : 0), -1};
  else if (scene == Scene::Duel && duel.phase == Duel::Go)
    f = {12, 9, 1, -1};
  else if (now < feedback.until)
    f = feedback.sample(now);
  else if (scene == Scene::Reaction && reaction.phase == Reaction::Wait)
    f = {3, 2, 0, -1};
  else if (scene == Scene::Duel && duel.phase == Duel::Wait)
    f = {0, 0, 2, -1};
  else if (scene == Scene::Duel &&
           (duel.phase == Duel::Invite || duel.phase == Duel::Offer ||
            duel.phase == Duel::Accept))
    f = {4, 0, 6, int(now / 220 % 6)};
  else if (session.pending || scene == Scene::Scan)
    f = {0, 3, 7, int(now / 180 % 6)};
  else if (scene == Scene::Maze)
    f = {0, 4, 5, int((mazeX + mazeY - 2) % 6)};
  else
    f = {1, 4, 2, int(now / 400 % 6)};
  return brightness(f, ledEnabled, dim);
}
void update_leds(uint64_t now) {
  if (now < ledAt)
    return;
  ledAt = now + 40;
  auto f = gameplay_light(now);
  // Retry even unchanged colors: an optional frame may have been skipped while
  // the previous asynchronous transfer was finishing.
  board::leds(f.r, f.g, f.b, f.active);
}
void draw() {
  board::Screen f{};
  str(f.title, "BADGE MARKET");
  str(f.subtitle, "TRADE COINS / PLAY MINIGAMES");
  str(f.footer, "UP/DOWN MOVE   A SELECT   B BACK");
  str(f.status, session.status);
  auto &w = session.world;
  auto *player = session.player >= 0 ? &w.p[session.player] : nullptr;
  switch (scene) {
  case Scene::Boot:
    str(f.title, "BADGE MARKET");
    str(f.rows[0], "SOLO MARKET");
    str(f.rows[1], "HOST NEARBY MARKET");
    str(f.rows[2], "JOIN NEARBY MARKET");
    str(f.subtitle, selected == 0   ? "PLAY ON YOUR OWN BADGE"
                    : selected == 1 ? "OPEN A MARKET FOR YOUR FRIENDS"
                                    : "FIND A FRIEND'S MARKET");
    f.selected = selected;
    str(f.footer, "UP/DOWN CHOOSE   A ENTER");
    if (reset_problem() && !std::strcmp(session.status, "CHOOSE A MARKET"))
      str(f.status, reset_problem());
    break;
  case Scene::Scan:
    str(f.title, "NEARBY MARKETS");
    str(f.subtitle, "CHOOSE THE CODE ON YOUR FRIEND'S BADGE");
    if (!session.peerCount)
      str(f.rows[1], "WAITING FOR A HOST...");
    for (int row = 0; row < 6; row++) {
      int i = selected / 6 * 6 + row;
      if (i < session.peerCount)
        format(f.rows[row], sizeof f.rows[row], "HOST %02X%02X  %d DBM",
               session.peers[i].mac.b[4], session.peers[i].mac.b[5],
               session.peers[i].rssi);
    }
    f.selected = session.peerCount ? selected % 6 : -1;
    str(f.footer, "UP/DOWN CHOOSE  A JOIN  B BACK");
    break;
  case Scene::Hub:
    if (session.mode == Mode::Solo)
      str(f.title, "SOLO MARKET");
    else {
      const auto &host =
          session.mode == Mode::Host ? session.self : session.host;
      format(f.title, sizeof f.title, "%s %02X%02X",
             session.mode == Mode::Host ? "HOST" : "MARKET", host.b[4],
             host.b[5]);
    }
    if (player)
      format(f.subtitle, sizeof f.subtitle, "%s  SOL %lu.%03lu  REP %u",
             "WALLET", (unsigned long)player->balance / 1000,
             (unsigned long)player->balance % 1000, player->rep);
    str(f.rows[0], "MARKET");
    str(f.rows[1], "MINIGAMES");
    str(f.rows[2], "PORTFOLIO");
    str(f.rows[3], "LAUNCH A COIN");
    str(f.rows[4], "BADGE DUELS");
    str(f.rows[5], "PROFILE / SETTINGS");
    f.selected = selected;
    str(f.footer, "UP/DOWN MOVE   A OPEN   B LEAVE");
    if (session.mode == Mode::Host &&
        !std::strcmp(session.status, "MARKET OPEN"))
      str(f.status, "FRIENDS: JOIN THE HOST CODE ABOVE");
    break;
  case Scene::Leave:
    str(f.title,
        session.mode == Mode::Host ? "CLOSE THIS MARKET?" : "LEAVE MARKET?");
    str(f.subtitle,
        session.mode == Mode::Host   ? "OTHER BADGES WILL LOSE THE CONNECTION"
        : session.mode == Mode::Solo ? "YOUR PROGRESS WILL BE SAVED"
                                     : "YOUR WALLET STAYS WITH THE HOST");
    str(f.rows[0], "KEEP PLAYING");
    str(f.rows[1], session.mode == Mode::Host   ? "SAVE AND CLOSE"
                   : session.mode == Mode::Solo ? "SAVE AND LEAVE"
                                                : "LEAVE MARKET");
    if (session.pending)
      str(f.rows[3], "LAST TRADE MAY COMPLETE");
    f.selected = selected;
    str(f.footer, "UP/DOWN CHOOSE   A SELECT   B CANCEL");
    break;
  case Scene::Market:
    str(f.title, "MARKET");
    format(f.subtitle, sizeof f.subtitle, "%u COINS / EPOCH %u", w.coins,
           w.epoch);
    if (!w.coins) {
      str(f.subtitle, "NO COINS YET / CREATE ONE TO START");
      str(f.rows[0], "LAUNCH FIRST COIN");
    }
    for (int row = 0; row < 6; row++) {
      int i = selected / 6 * 6 + row;
      if (i < w.coins)
        format(f.rows[row], sizeof f.rows[row], "%-5s %s %u", w.c[i].symbol,
               w.c[i].rugged ? "RUG" : "SUP", w.c[i].supply);
    }
    f.selected = w.coins ? selected % 6 : 0;
    str(f.footer, "UP/DOWN CHOOSE   A OPEN   B BACK");
    break;
  case Scene::Coin: {
    auto &c = w.c[coinId - 1];
    str(f.title, c.symbol);
    format(f.subtitle, sizeof f.subtitle, "SUPPLY %u / OWN %u / MEME %u",
           c.supply, player ? owned(*player, coinId) : 0, c.meme);
    str(f.rows[0], "BUY - UNAVAILABLE");
    str(f.rows[1], "SELL - UNAVAILABLE");
    auto price = cost(c.supply, amount);
    if (price >= 0 && !c.rugged) {
      price += (price + 49) / 50;
      format(f.rows[0], sizeof f.rows[0], "BUY  %lld.%03lld SOL",
             (long long)price / 1000, (long long)price % 1000);
    }
    auto sell = cost(c.supply - amount, amount);
    if (sell >= 0 && player && owned(*player, coinId) >= amount) {
      sell -= c.rugged ? 0 : (sell + 49) / 50;
      format(f.rows[1], sizeof f.rows[1], "SELL %lld.%03lld SOL",
             (long long)sell / 1000, (long long)sell % 1000);
    }
    format(f.rows[2], sizeof f.rows[2], "QUANTITY: %d", amount);
    bool owner = c.creator == session.player;
    if (owner)
      str(f.rows[3], "CREATOR CONTROLS");
    str(f.rows[owner ? 4 : 3], coinReturn == Scene::Portfolio
                                   ? "BACK TO PORTFOLIO"
                                   : "BACK TO MARKET");
    f.selected = selected;
    if (c.rugged)
      f.accent = board::Red;
    break;
  }
  case Scene::Quantity:
    str(f.title, "TRADE QUANTITY");
    str(f.subtitle, "SET HOW MANY TOKENS TO BUY OR SELL");
    format(f.rows[1], sizeof f.rows[1], "%d TOKENS", amount);
    str(f.rows[3], "UP MORE / DOWN FEWER");
    str(f.rows[4], "A DONE");
    str(f.footer, "UP/DOWN ADJUST   A DONE   B BACK");
    break;
  case Scene::Create:
    str(f.title, "LAUNCH A COIN");
    format(f.subtitle, sizeof f.subtitle, "SYMBOL %s / COST 2 SOL", symbol);
    for (int i = 0; i < 5; i++)
      format(f.rows[i], sizeof f.rows[i], "LETTER %d: %c%s", i + 1, symbol[i],
             editingLetter && selected == i ? " < EDIT" : "");
    str(f.rows[5], "LAUNCH FOR 2 SOL");
    f.selected = selected;
    if (editingLetter)
      str(f.footer, "UP/DOWN LETTER   A DONE   B CANCEL");
    break;
  case Scene::Portfolio:
    str(f.title, "PORTFOLIO");
    str(f.subtitle, "YOUR COINS / A OPENS TRADING");
    if (player) {
      for (int i = 0; i < 4; i++) {
        auto &h = player->holdings[i];
        if (!h.coin)
          str(f.rows[i], "EMPTY SLOT");
        if (h.coin)
          format(f.rows[i], sizeof f.rows[i], "%-5s   %u TOKENS",
                 w.c[h.coin - 1].symbol, h.quantity);
      }
      format(f.rows[5], sizeof f.rows[5], "WALLET %lu.%03lu SOL",
             (unsigned long)player->balance / 1000,
             (unsigned long)player->balance % 1000);
    }
    f.selected = selected;
    break;
  case Scene::Creator: {
    auto &c = w.c[coinId - 1];
    str(f.title, "CREATOR CONTROLS");
    format(f.subtitle, sizeof f.subtitle, "CREATOR FEES %lu.%03lu SOL",
           (unsigned long)c.creatorFees / 1000,
           (unsigned long)c.creatorFees % 1000);
    str(f.rows[0], "WITHDRAW 10%");
    str(f.rows[1], "WITHDRAW 25%");
    str(f.rows[2], "WITHDRAW 50%");
    str(f.rows[3], "PULL THE RUG...");
    f.selected = selected;
    str(f.rows[5], "RESERVES STAY FUNDED");
    break;
  }
  case Scene::Rug:
    f.accent = board::Red;
    str(f.title, "CONFIRM RUG");
    str(f.subtitle, "WITHDRAW ALL FEES / CLOSE NEW BUYS");
    str(f.rows[0], "35 REPUTATION LOST");
    str(f.rows[1], "3 EPOCH COOLDOWN");
    str(f.rows[3],
        rugArmed ? "HOLD A FOR 3 SECONDS" : "RELEASE A, THEN HOLD A");
    if (rugAt)
      format(f.rows[4], sizeof f.rows[4], "HOLDING... %u",
             unsigned(std::min(uint64_t(3), (board::ms() - rugAt) / 1000)));
    str(f.footer, "RELEASE TO CANCEL   B BACK");
    break;
  case Scene::Games:
    str(f.title, "MINIGAMES");
    str(f.rows[0], "REACTION TRADER");
    str(f.rows[1], "TILT VAULT");
    str(f.rows[3], "REACTION: UP TO 2 SOL");
    str(f.rows[4], "6 SOL PER EPOCH CAP");
    str(f.rows[5], "MAZE: PRACTICE ONLY");
    f.selected = selected;
    break;
  case Scene::Reaction: {
    str(f.footer, "A BUY   B SELL   UP HOLD   START HUB");
    str(f.title, "REACTION TRADER");
    str(f.subtitle, "A BUY / B SELL / UP HOLD - WAIT FOR CUE");
    const char *cues[] = {"BUY!", "SELL!", "HOLD!"};
    str(f.rows[0], reaction.phase == Reaction::Wait   ? "WAIT..."
                   : reaction.phase == Reaction::Go   ? cues[reaction.key]
                   : reaction.phase == Reaction::Done ? "ROUND COMPLETE"
                                                      : "GET READY");
    format(f.rows[2], sizeof f.rows[2], "ROUND %u / 8",
           std::min(8u, reaction.round));
    format(f.rows[3], sizeof f.rows[3], "CORRECT %u / 8", reaction.score);
    if (reaction.phase == Reaction::Done)
      str(f.rows[5],
          session.pending ? "VERIFYING REWARD..." : "B BACK TO MINIGAMES");
    if (reaction.phase == Reaction::Done)
      str(f.footer, "B MINIGAMES   START HUB   HOME LEAVE");
    if (reaction.phase == Reaction::Go)
      f.accent = reaction.key == 0   ? board::Green
                 : reaction.key == 1 ? board::Red
                                     : board::Blue;
    break;
  }
  case Scene::Maze: {
    str(f.footer, "TILT OR D-PAD MOVE   B BACK");
    str(f.title, "TILT VAULT");
    str(f.subtitle, mazeWon ? "VAULT OPEN! / PRACTICE, NO SOL"
                            : "TILT OR D-PAD / MOVE @ TO $");
    static const int walls[] = {127, 65, 93, 81, 87, 65, 127};
    for (int y = 1; y <= 5; y++) {
      char row[16]{};
      for (int x = 1; x <= 5; x++) {
        row[(x - 1) * 2] = x == mazeX && y == mazeY ? '@'
                           : x == 5 && y == 5       ? '$'
                           : walls[y] & (1 << x)    ? '#'
                                                    : '.';
        row[(x - 1) * 2 + 1] = ' ';
      }
      str(f.rows[y - 1], row);
    }
    break;
  }
  case Scene::Nearby:
    str(f.title, "BADGE DUELS");
    str(f.subtitle, "LOCAL REACTION TIMES / BRAGGING RIGHTS");
    for (int row = 0; row < 6; row++) {
      int p = nearby(selected / 6 * 6 + row);
      if (p >= 0)
        format(f.rows[row], sizeof f.rows[row], "TRADER %02X%02X  REP %u",
               w.p[p].mac.b[4], w.p[p].mac.b[5], w.p[p].rep);
    }
    if (w.players < 2)
      str(f.rows[1], "NO OTHER TRADERS YET");
    else
      f.selected = selected % 6;
    str(f.footer, "UP/DOWN CHOOSE   A DUEL   B BACK");
    break;
  case Scene::Duel:
    str(f.footer, "A ACCEPT / REACT   B CANCEL");
    str(f.title, "BADGE DUEL");
    str(f.subtitle, "A REACT / B CANCEL / NO SOL AT STAKE");
    str(f.rows[0], duel.phase == Duel::Invite ? "A ACCEPT CHALLENGE"
                   : duel.phase == Duel::Go   ? "HONK! PRESS A!"
                   : duel.phase == Duel::Wait ? "WAIT FOR HONK..."
                   : duel.phase == Duel::Done
                       ? (duel.mine == duel.theirs  ? "TIE!"
                          : duel.mine < duel.theirs ? "YOU WIN!"
                                                    : "THEY WIN!")
                   : duel.phase == Duel::Cancelled ? "DUEL TIMED OUT"
                                                   : "WAITING FOR PEER...");
    if (duel.haveMine)
      format(f.rows[2], sizeof f.rows[2], "YOU  %s%u",
             duel.mine == 65535 ? "FOUL " : "MS ",
             duel.mine == 65535 ? 0 : duel.mine);
    if (duel.haveTheirs)
      format(f.rows[3], sizeof f.rows[3], "THEM %s%u",
             duel.theirs == 65535 ? "FOUL " : "MS ",
             duel.theirs == 65535 ? 0 : duel.theirs);
    break;
  case Scene::Profile:
    str(f.title, "PROFILE / SETTINGS");
    if (player)
      format(f.subtitle, sizeof f.subtitle, "REPUTATION %u / RUGS %u",
             player->rep, player->rugs);
    str(f.rows[0], "SAVE NOW");
    str(f.rows[1], ledEnabled ? "LEDS: ON" : "LEDS: OFF");
    str(f.rows[2], dim ? "BRIGHTNESS: DIM" : "BRIGHTNESS: NORMAL");
    str(f.rows[3], "BACK TO HUB");
    str(f.rows[4], saveStatus);
    format(f.rows[5], sizeof f.rows[5], "FREE RAM %u KB",
           unsigned(esp_get_free_heap_size() / 1024));
    f.selected = selected;
    break;
  }
  board::render(f);
}
void console(uint64_t now) {
  static char line[64];
  static size_t count = 0;
  uint8_t data[32];
  int n = usb_serial_jtag_read_bytes(data, sizeof data, 0);
  for (int i = 0; i < n; i++) {
    char c = data[i];
    if (c == '\r' || c == '\n') {
      if (!count)
        continue;
      line[count] = 0;
      count = 0;
      if (!std::strcmp(line, "help"))
        printf("status | save | press A/B/HOME/DOWN/LEFT/RIGHT/UP/AUX/START | "
               "down A | up A\n");
      else if (!std::strcmp(line, "status"))
        printf("BM mode=%u player=%d coins=%u heap=%lu min=%lu drops=%lu "
               "radio=%u tx_qdbm=%d reset=%d scene=%u selected=%d status=%s\n",
               unsigned(session.mode), session.player, session.world.coins,
               (unsigned long)esp_get_free_heap_size(),
               (unsigned long)esp_get_minimum_free_heap_size(),
               (unsigned long)radio::dropped(), unsigned(radio::active()),
               radio::tx_power(), int(esp_reset_reason()), unsigned(scene),
               selected, session.status);
      else if (!std::strcmp(line, "save"))
        checkpoint();
      else {
        const char *names[] = {"A",     "B",  "HOME", "DOWN", "LEFT",
                               "RIGHT", "UP", "AUX",  "START"};
        char verb[8]{}, key[12]{};
        if (sscanf(line, "%7s %11s", verb, key) == 2)
          for (int b = 0; b < 9; b++)
            if (!std::strcmp(names[b], key)) {
              if (!std::strcmp(verb, "press")) {
                injected |= 1 << b;
                pulseEnd = now + 100;
              } else if (!std::strcmp(verb, "down"))
                injected |= 1 << b;
              else if (!std::strcmp(verb, "up"))
                injected &= ~(1 << b);
            }
      }
      printf("bm> ");
      fflush(stdout);
    } else if (count < sizeof(line) - 1)
      line[count++] = c;
    else
      count = 0;
  }
}
} // namespace
extern "C" void app_main() {
  ESP_LOGI("boot", "reset_reason=%d", int(esp_reset_reason()));
  ESP_ERROR_CHECK(board::init());
  saveOk = storage::init();
  if (!saveOk)
    saveStatus = "SAVE STORAGE UNAVAILABLE";
  bm::Mac mac;
  ESP_ERROR_CHECK(radio::init(mac));
  session.init(mac, radio::send, [](void *) { return esp_random(); }, nullptr);
  duel.init(session);
  usb_serial_jtag_driver_config_t usb{};
  usb.rx_buffer_size = 256;
  usb.tx_buffer_size = 512;
  ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb));
  printf("\nBadge Market native 0.2.4 / ESP-NOW channel 6\nType help for "
         "console commands.\nbm> ");
  while (true) {
    uint64_t now = board::ms();
    console(now);
    if (pulseEnd && now >= pulseEnd) {
      injected = 0;
      pulseEnd = 0;
    }
    uint16_t current = board::buttons(now) | injected,
             pressed = current & ~held;
    held = current;
    for (int i = 0; i < 9; i++)
      if (pressed & (1 << i))
        button(i, now);
    radio::Packet packet;
    for (int i = 0; i < 8 && radio::receive(packet); i++)
      session.receive(packet.from, packet.rssi, packet.data, packet.size, now);
    session.tick(now);
    duel.tick(now);
    if (scene == Scene::Scan && session.player >= 0)
      go(Scene::Hub);
    if (wantTicket && !session.pending && scene != Scene::Leave) {
      wantTicket = false;
      if (scene == Scene::Games && session.last.code == Error::Ok &&
          session.last.ticket) {
        reaction.start(session.last.ticket, now);
        claimSent = false;
        go(Scene::Reaction);
      }
    }
    if (scene == Scene::Reaction) {
      auto previousPhase = reaction.phase;
      dirty |= reaction.tick(now);
      if (previousPhase == Reaction::Go && reaction.phase == Reaction::Pause)
        feedback.trigger(Glow::Error, now);
      if (reaction.phase == Reaction::Done && !claimSent) {
        Request r{};
        r.op = Op::Claim;
        std::memcpy(r.proof, reaction.proof, 16);
        claimSent = session.act(r, now);
      }
    }
    if (duel.phase == Duel::Invite && scene != Scene::Duel &&
        scene != Scene::Leave)
      go(Scene::Duel);
    tick_rug(now);
    if (scene == Scene::Maze && !mazeWon && now >= mazeAt) {
      mazeAt = now + 130;
      int ax = 0, ay = 0;
      board::accel(ax, ay);
      int dx = ax > 180    ? 1
               : ax < -180 ? -1
                           : 0,
          dy = ay > 180    ? 1
               : ay < -180 ? -1
                           : 0;
      if (held & (1 << board::Left))
        dx = -1;
      if (held & (1 << board::Right))
        dx = 1;
      if (held & (1 << board::Up))
        dy = -1;
      if (held & (1 << board::Down))
        dy = 1;
      static const int walls[] = {127, 65, 93, 81, 87, 65, 127};
      int x = mazeX + dx, y = mazeY + dy;
      if (x >= 1 && x <= 5 && !(walls[mazeY] & (1 << x)))
        mazeX = x;
      if (y >= 1 && y <= 5 && !(walls[y] & (1 << mazeX)))
        mazeY = y;
      mazeWon = mazeX == 5 && mazeY == 5;
      if (mazeWon)
        feedback.trigger(Glow::Win, now);
      dirty |= dx || dy;
    }
    if (session.changed || duel.changed) {
      dirty = true;
      session.changed = duel.changed = false;
    }
    unsigned rugged = 0;
    for (unsigned i = 0; i < session.world.coins; i++)
      if (session.world.c[i].rugged)
        rugged |= 1u << i;
    if (rugged & ~rugsSeen) {
      feedback.trigger(Glow::Rug, now);
      session.status = "RUG ALERT! HOLDERS CAN STILL SELL";
      dirty = true;
    }
    rugsSeen = rugged;
    if (session.authority() && now >= saveAt) {
      checkpoint();
      saveAt = now + 30000;
    }
    if (dirty && now >= paintAt) {
      draw();
      dirty = false;
      paintAt = board::ms() + 80;
    }
    observe_feedback(now);
    update_leds(board::ms());
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
