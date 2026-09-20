// Compile the actual firmware controller against host HAL stubs. This tests
// rendered options and button routes, not a second implementation of the UI.
#include "../main/main.cpp"
#include "display.hpp"
#include <fstream>
namespace board {
Screen lastFrame;
esp_err_t init() { return ESP_OK; }
uint16_t buttons(uint64_t) { return 0; }
bool accel(int &, int &) { return false; }
void render(const Screen &f) { lastFrame = f; }
void leds(uint8_t, uint8_t, uint8_t, int) {}
uint64_t ms() { return 10000; }
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
int tx_power() { return running ? 20 : -1; }
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
  draw();
  std::ofstream out(std::string("build/native-ui-") + name + ".ppm",
                    std::ios::binary);
  assert(out.good());
  out << "P6\n320 240\n255\n";
  uint16_t pixels[320 * 16];
  board::display::Painter painter{pixels};
  for (int y = 0; y < 240; y += 16) {
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
int main() {
  session.init(
      Mac{{2, 0, 0, 0, 0, 1}}, radio::send, [](void *) { return 42u; },
      nullptr);
  duel.init(session);
  go(Scene::Boot);
  preview("home");
  assert(!radio::active());
  click(board::Up); // Join activates radio; backing out powers it down.
  click(board::A);
  assert(scene == Scene::Scan && radio::active());
  click(board::B);
  assert(scene == Scene::Boot && !radio::active());
  click(board::A);
  assert(session.mode == Mode::Solo && !radio::active());
  session.init(
      Mac{{2, 0, 0, 0, 0, 1}}, radio::send, [](void *) { return 42u; },
      nullptr);
  go(Scene::Boot);
  click(board::Down);
  radio::failStart = true;
  click(board::A);
  assert(scene == Scene::Boot && session.mode == Mode::Menu &&
         !radio::active());
  assert(!std::strcmp(board::lastFrame.status, "RADIO ERROR - TRY AGAIN"));
  radio::failStart = false;
  click(board::A);
  assert(session.mode == Mode::Host && scene == Scene::Hub && radio::active());
  preview("host");
  assert(selected == 0 && board::lastFrame.selected == 0);
  click(board::Down);
  assert(scene == Scene::Hub && selected == 1);
  click(board::A);
  assert(scene == Scene::Games);
  click(board::B);
  assert(scene == Scene::Hub && selected == 1);
  click(board::Up);
  click(board::Up);
  assert(selected == 5);
  click(board::A);
  assert(scene == Scene::Profile);
  bool old = ledEnabled;
  click(board::Down);
  click(board::A);
  assert(ledEnabled != old);
  click(board::B);
  assert(scene == Scene::Hub && selected == 5);
  go(Scene::Hub, 3);
  click(board::A);
  assert(scene == Scene::Create);
  click(board::B);
  assert(scene == Scene::Hub && selected == 3);
  click(board::A);
  char oldLetter = symbol[0];
  click(board::A);
  assert(editingLetter);
  click(board::Up);
  assert(symbol[0] != oldLetter);
  click(board::B);
  assert(!editingLetter && symbol[0] == oldLetter && scene == Scene::Create);
  click(board::Up);
  assert(selected == 5);
  click(board::A);
  assert(scene == Scene::Market && session.world.coins == 1);
  click(board::A);
  assert(scene == Scene::Coin);
  preview("trade");
  auto balance = session.world.p[0].balance;
  click(board::Down);
  assert(scene == Scene::Coin && selected == 1);
  assert(session.world.p[0].balance ==
         balance); // Selecting SELL alone does nothing.
  click(board::B);
  assert(scene == Scene::Market);
  click(board::A);
  click(board::Down);
  click(board::Down);
  click(board::A);
  assert(scene == Scene::Quantity);
  click(board::Up);
  assert(amount == 2);
  click(board::A);
  assert(scene == Scene::Coin && selected == 2);
  click(board::Up);
  click(board::Up);
  click(board::A);
  assert(owned(session.world.p[0], 1) == 2);
  click(board::Down);
  click(board::A);
  assert(owned(session.world.p[0], 1) == 0);
  click(board::Down);
  click(board::Down);
  click(board::A);
  assert(scene == Scene::Creator);
  click(board::B);
  assert(scene == Scene::Coin);
  click(board::Start);
  assert(scene == Scene::Hub);
  for (int i = 0; i < 100; i++) {
    click(board::Up);
    click(board::Down);
    draw();
  }
  go(Scene::Creator);
  selected = 3;
  held = 1 << board::A;
  click(board::A);
  assert(scene == Scene::Rug);
  tick_rug(11000);
  tick_rug(15000);
  assert(
      !session.world.c[0].rugged); // Entry press cannot start destructive hold.
  held = 0;
  tick_rug(15010);
  held = 1 << board::A;
  tick_rug(15020);
  tick_rug(17000);
  assert(!session.world.c[0].rugged);
  held = 0;
  tick_rug(17010);
  held = 1 << board::A;
  tick_rug(18000);
  tick_rug(21001);
  assert(session.world.c[0].rugged && scene == Scene::Coin);
  feedback = Feedback{};
  ledCompleted = session.completed;
  scene = Scene::Hub;
  session.last.code = Error::Ok;
  session.completedOp = Op::Buy;
  session.completed++;
  observe_feedback(30000);
  assert(feedback.effect == Glow::Buy);
  auto light = feedback.sample(30100);
  assert(light.g > light.r && light.g > light.b);
  session.completedOp = Op::Sell;
  observe_feedback(30200);
  assert(feedback.effect ==
         Glow::Buy); // No new completion, no repeated/false success.
  session.completed++;
  observe_feedback(30300);
  assert(feedback.effect == Glow::Sell);
  session.last.code = Error::Funds;
  session.completed++;
  observe_feedback(30400);
  assert(feedback.effect == Glow::Error);
  scene = Scene::Reaction;
  reaction.phase = Reaction::Go;
  reaction.key = 2;
  ledEnabled = true;
  dim = false;
  light = gameplay_light(30410);
  assert(light.b == 12 && light.r == 0 &&
         light.g == 0); // Cue overrides error animation.
  dim = true;
  light = gameplay_light(30410);
  assert(light.b == 3);
  ledEnabled = false;
  light = gameplay_light(30410);
  assert(!light.r && !light.g && !light.b);
  ledEnabled = true;
  dim = false;
  scene = Scene::Duel;
  duel.phase = Duel::Go;
  light = gameplay_light(30410);
  assert(light.r == 12 && light.g == 9);
  feedback.trigger(Glow::Rug, 31000);
  feedback.trigger(Glow::Navigate, 31100);
  assert(feedback.effect == Glow::Rug);
  for (unsigned t = 31000; t < 35000; t += 10) {
    auto l = feedback.sample(t);
    assert(l.r <= 12 && l.g <= 12 && l.b <= 12);
  }
  light = feedback.sample(35000);
  assert(!light.r && !light.g && !light.b);
  assert(session.world.valid());
  go(Scene::Hub);
  click(board::B);
  assert(scene == Scene::Leave && selected == 0 && radio::active());
  preview("leave");
  click(board::B);
  assert(scene == Scene::Hub && session.mode == Mode::Host);
  go(Scene::Quantity);
  click(board::Home);
  assert(scene == Scene::Leave);
  click(board::B);
  assert(scene == Scene::Quantity);
  click(board::B);
  assert(scene == Scene::Coin && selected == 2);
  go(Scene::Portfolio, 0);
  // The earlier sell left an empty slot; buy once to exercise this route.
  session.world.c[0].rugged = false;
  action(Op::Buy, 1, 1);
  click(board::A);
  assert(scene == Scene::Coin);
  click(board::B);
  assert(scene == Scene::Portfolio && selected == 0);
  click(board::Home);
  click(board::Down);
  storage::saveSucceeds = false;
  click(board::A);
  assert(scene == Scene::Leave && session.mode == Mode::Host &&
         radio::active());
  assert(!std::strcmp(session.status, "SAVE FAILED - TRY AGAIN"));
  storage::saveSucceeds = true;
  wantTicket = claimSent = true;
  click(board::A);
  assert(scene == Scene::Boot && session.mode == Mode::Menu &&
         session.player == -1);
  assert(!radio::active() && !wantTicket && !claimSent && !session.pending);
  assert(duel.phase == Duel::Idle && reaction.phase == Reaction::Idle);
  click(board::A);
  assert(scene == Scene::Hub && session.mode == Mode::Solo && !radio::active());
  click(board::A); // Empty market offers a direct create action.
  assert(scene == Scene::Market);
  click(board::A);
  assert(scene == Scene::Create);
  click(board::A);
  click(board::Home);
  click(board::B);
  assert(scene == Scene::Create && editingLetter);
  click(board::B);
  click(board::B);
  assert(scene == Scene::Market);
  // Leaving a client must not write its copy as a local host checkpoint.
  session.mode = Mode::Client;
  session.pending = true;
  auto saves = storage::saves;
  click(board::Home);
  click(board::Down);
  click(board::A);
  assert(scene == Scene::Boot && !session.pending && storage::saves == saves);
  assert(session.socialContext == &duel && duel.session == &session);
  puts("PASS: actual native Host startup and Up/Down/A/B menu routes, create "
       "editor, quantity, buy/sell, settings, Home/B exit, save failure, "
       "client leave, renderer.");
}
