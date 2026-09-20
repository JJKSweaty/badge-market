// Compile the actual firmware controller against host HAL stubs. This tests
// rendered options and button routes, not a second implementation of the UI.
#include "../main/main.cpp"
#include "display.hpp"
#include <fstream>
#include <random>
#include <vector>
namespace board {
Screen lastFrame;
esp_err_t init() { return ESP_OK; }
uint16_t buttons(uint64_t) { return 0; }
bool accel(int &, int &) { return false; }
void render(const Screen &f) { lastFrame = f; }
void leds(uint8_t, uint8_t, uint8_t, int) {}
uint64_t clockMs = 10000;
uint64_t ms() { return clockMs; }
} // namespace board
namespace radio {
bool running = false, failStart = false;
esp_err_t init(Mac &) { return ESP_OK; }
esp_err_t enable() {
  if (failStart)
    return 1;
  running = true;
  return ESP_OK;
}
esp_err_t disable() {
  running = false;
  return ESP_OK;
}
bool active() { return running; }
int tx_power() { return running ? 8 : -1; }
bool send(void *, const uint8_t *, size_t) { return true; }
bool receive(Packet &) { return false; }
uint32_t dropped() { return 0; }
} // namespace radio
namespace storage {
bool saveSucceeds = true;
unsigned saves = 0;
bool init() { return true; }
bool load(bool, Market &) { return false; }
bool save(bool, Market &) {
  ++saves;
  return saveSucceeds;
}
} // namespace storage
void click(int b) {
  button(b, board::ms());
  draw();
}
void preview(const char *name) {
  board::clockMs += 200;
  draw();
  std::ofstream out(std::string("build/native-ui-") + name + ".ppm",
                    std::ios::binary);
  assert(out.good());
  out << "P6\n320 240\n255\n";
  uint16_t pixels[320 * board::display::StripeH];
  board::display::Painter painter{pixels};
  for (int y = 0; y < 240; y += board::display::StripeH) {
    painter.paint(board::lastFrame, y);
    for (auto v : pixels) {
      uint16_t rgb = (v << 8) | (v >> 8);
      const char c[] = {char(((rgb >> 11) & 31) * 255 / 31),
                        char(((rgb >> 5) & 63) * 255 / 63),
                        char((rgb & 31) * 255 / 31)};
      out.write(c, 3);
    }
  }
}
void advance(unsigned ms) {
  for (unsigned t = 0; t < ms; t += 20) {
    board::clockMs += 20;
    session.tick(board::clockMs);
    game_tick(board::clockMs);
  }
  draw();
}
void verify_damage() {
  board::display::Damage damage;
  std::vector<uint16_t> full(320 * 240), incremental(320 * 240);
  uint16_t pixels[320 * board::display::StripeH];
  board::display::Painter painter{pixels};
  std::mt19937 rng(7);
  for (int frame = 0; frame < 400; frame++) {
    board::Screen f{};
    f.custom = true;
    f.background = frame % 17 ? board::Paper : board::Surface;
    f.translate = frame % 5;
    for (unsigned i = 0, n = rng() % 30; i < n; i++) {
      f.box(int(rng() % 350) - 15, int(rng() % 270) - 15, 1 + rng() % 50,
            1 + rng() % 60, uint16_t(rng()), rng() % 2);
      if (i % 3 == 0)
        f.text(rng() % 280, rng() % 240, "DRAW TEST", board::Ink,
               1 + rng() % 2);
    }
    auto mask = damage.update(f);
    for (int y = 0; y < 240; y += board::display::StripeH) {
      painter.paint(f, y);
      std::copy_n(pixels, 320 * board::display::StripeH,
                  full.begin() + 320 * y);
      if (mask & (1u << (y / board::display::StripeH)))
        std::copy_n(pixels, 320 * board::display::StripeH,
                    incremental.begin() + 320 * y);
    }
    assert(full == incremental);
    assert(damage.update(f) == 0);
  }
}
int main() {
  verify_damage();
  session.init(
      Mac{{2, 0, 0, 0, 0, 1}}, radio::send, [](void *) { return 42u; },
      nullptr);
  go(Scene::Boot);
  preview("home");
  click(board::Up);
  click(board::A);
  assert(scene == Scene::Scan && radio::active());
  click(board::B);
  assert(scene == Scene::Boot && !radio::active());
  click(board::Down);
  radio::failStart = true;
  click(board::A);
  assert(scene == Scene::Boot);
  radio::failStart = false;
  click(board::A);
  assert(scene == Scene::Hub && session.mode == Mode::Host);
  preview("host");
  go(Scene::Create, 5);
  click(board::A);
  assert(session.world.coins == 1);
  click(board::A);
  assert(scene == Scene::Coin);
  auto balance = session.world.p[0].balance;
  click(board::A);
  assert(scene == Scene::Quantity && session.world.p[0].balance == balance);
  click(board::Up);
  click(board::A);
  assert(owned(session.world.p[0], 1) == 2);
  preview("trade");
  click(board::B);
  assert(scene == Scene::Quantity && !buying);
  click(board::A);
  assert(!owned(session.world.p[0], 1));
  go(Scene::Creator, 3);
  held = 1 << board::A;
  click(board::A);
  tick_rug(15000);
  assert(!session.world.c[0].rugged);
  held = 0;
  tick_rug(15010);
  held = 1 << board::A;
  tick_rug(15020);
  tick_rug(18021);
  held = 0;
  assert(session.world.c[0].rugged);
  go(Scene::Games);
  preview("games");
  click(board::A);
  advance(20);
  assert(scene == Scene::Word);
  click(board::Home);
  click(board::Down);
  storage::saveSucceeds = false;
  click(board::A);
  assert(scene == Scene::Leave && activeGame == Game::Word);
  click(board::B);
  assert(scene == Scene::Word && activeGame == Game::Word);
  storage::saveSucceeds = true;
  preview("word");
  click(board::Up);
  assert(game.word.guesses[0][0] == 'A');
  click(board::Down);
  assert(game.word.guesses[0][0] == 'Z');
  click(board::B);
  assert(!game.word.guesses[0][0]);
  held = 1 << board::Up;
  advance(900);
  held = 0;
  assert(game.word.guesses[0][0] >= 'C');
  click(board::Start);
  assert(scene == Scene::Pause);
  auto tick = gameAt;
  advance(2000);
  click(board::B);
  assert(scene == Scene::Word && gameAt > tick);
  std::memcpy(game.word.guesses[0], game.word.answer, 6);
  click(board::A);
  advance(1400);
  assert(scene == Scene::Result);
  advance(40);
  assert(rewardSettled && rewardValue == 10000);
  preview("word-result");
  click(board::A);
  advance(20);
  assert(scene == Scene::Word && session.world.p[0].ticket);
  std::memcpy(game.word.guesses[0], game.word.answer, 6);
  click(board::A);
  advance(1400);
  advance(40);
  assert(rewardSettled &&
         rewardValue ==
             0); // Immediate restart cannot duplicate the puzzle payout.

  click(board::B);
  assert(scene == Scene::Games && activeGame == Game::None);
  advance(20);
  click(board::Down);
  click(board::A);
  advance(20);
  assert(scene == Scene::Run);
  advance(2200);
  preview("run");
  click(board::Home);
  assert(scene == Scene::Leave);
  auto rt = game.run.tick;
  advance(500);
  assert(game.run.tick == rt);
  click(board::B);
  assert(scene == Scene::Run);
  advance(42000);
  advance(1000);
  assert(scene == Scene::Result && rewardSettled);
  preview("run-result");
  click(board::B);
  advance(20);
  go(Scene::Games, 2);
  click(board::A);
  assert(scene == Scene::Bomb);
  click(board::A);
  assert(game.bomb.phase == Bomb::Lobby);
  preview("bomb-lobby");
  click(board::B);
  assert(scene == Scene::Games && !session.gameReceive);
  for (int i = 0; i < 100; i++) {
    go(Scene::Games);
    advance(20);
    click(board::A);
    advance(20);
    assert(scene == Scene::Word);
    click(board::Start);
    click(board::Down);
    click(board::A);
    advance(20);
    assert(activeGame == Game::None);
  }
  go(Scene::Hub);
  click(board::Home);
  click(board::Down);
  storage::saveSucceeds = false;
  click(board::A);
  assert(scene == Scene::Leave && radio::active());
  storage::saveSucceeds = true;
  click(board::A);
  assert(scene == Scene::Boot && !radio::active());
  click(board::A);
  assert(session.mode == Mode::Solo && !radio::active());
  go(Scene::Create);
  click(board::A);
  click(board::Home);
  click(board::B);
  assert(editingLetter);
  click(board::B);
  session.mode = Mode::Client;
  session.pending = true;
  auto saved = storage::saves;
  click(board::Home);
  click(board::Down);
  click(board::A);
  assert(scene == Scene::Boot && storage::saves == saved && !session.pending);
  assert(!session.gameReceive && activeGame == Game::None);
  puts("PASS: actual controller, trade confirmations, word controls/rewards, "
       "Run pause/results, Bomb lifecycle, 100 scene cycles, save failure, "
       "client leave, previews.");
}
