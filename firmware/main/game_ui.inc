// Included inside the controller namespace; one active native game arena.
union GameArena {
  MemeWord word;
  Runner run;
  Bomb bomb;
  GameArena() {}
  ~GameArena() {}
} game;
Game activeGame = Game::None, requestedGame = Game::None;
Scene pauseReturn = Scene::Games;
uint64_t gameAt = 0, pausedAt = 0, repeatAt = 0, repeatStart = 0, resultAt = 0;
int repeatKey = -1;
uint32_t rewardValue = 0, claimObserved = 0, previousBest = 0;
uint16_t uploadOffset = 0, chunkPending = 0;
uint64_t claimRetryAt = 0;
uint8_t claimStage = 0;
bool cancelAfter = false, rewardSettled = false, newBest = false;
const char *resultTitle = "ROUND COMPLETE";
bool live_game() {
  if (activeGame == Game::Bomb)
    return game.bomb.phase == Bomb::Live || game.bomb.phase == Bomb::Countdown;
  return scene == Scene::Word || scene == Scene::Run;
}
void pause_game(uint64_t now) {
  if (activeGame != Game::None && activeGame != Game::Bomb && !pausedAt)
    pausedAt = now;
}
void resume_game(uint64_t now) {
  if (pausedAt) {
    gameAt += now - pausedAt;
    if (activeGame == Game::Word && game.word.revealAt)
      game.word.revealAt += now - pausedAt;
    pausedAt = 0;
  }
  repeatKey = -1;
}
void prepare_game_exit() {
  if (activeGame == Game::Bomb && game.bomb.host && live_game()) {
    game.bomb.phase = Bomb::Cancelled;
    game.bomb.message = "HOST LEFT THE GAME";
    game.bomb.announce(board::ms());
    game.bomb.send_state();
  }
}
void release_game() {
  if (activeGame == Game::Bomb) {
    game.bomb.exit(board::ms());
    game.bomb.~Bomb();
  }
  if (activeGame == Game::Word)
    game.word.~MemeWord();
  if (activeGame == Game::Run)
    game.run.~Runner();
  if (activeGame != Game::Bomb && (activeGame != Game::None || wantTicket))
    cancelAfter = true;
  activeGame = Game::None;
  requestedGame = Game::None;
  wantTicket = false;
  claimStage = 0;
  pausedAt = 0;
  repeatKey = -1;
  feedback = Feedback{};
  std::memset(static_cast<void *>(&game), 0, sizeof game);
}
void open_game(Game which, uint64_t now) {
  if (session.pending) {
    session.status = "WAIT FOR CURRENT REQUEST";
    return;
  }
  if (saveAt <= now && session.authority()) {
    checkpoint();
    saveAt = now + 30000;
  }
  if (which == Game::Bomb) {
    activeGame = Game::Bomb;
    new (&game.bomb) Bomb{};
    game.bomb.enter(session, now);
    go(Scene::Bomb);
    return;
  }
  if (session.player < 0)
    return;
  if (session.world.p[session.player].ticket) {
    Request cancel{};
    cancel.op = Op::CancelGame;
    session.act(cancel, now);
    session.status = "CLEARING OLD ROUND - PRESS A AGAIN";
    return;
  }
  cancelAfter = false;
  Request r{};
  r.op = Op::Ticket;
  r.coin = uint8_t(which);
  requestedGame = which;
  wantTicket = session.act(r, now);
  claimObserved = session.completed;
  session.status = "PREPARING ROUND";
  dirty = true;
}
void show_result(uint64_t now, const char *title) {
  resultTitle = title;
  resultAt = now;
  rewardValue = 0;
  rewardSettled = false;
  claimStage = 1;
  uploadOffset = chunkPending = 0;
  claimRetryAt = 0;
  claimObserved = session.completed;
  go(Scene::Result);
  feedback.trigger((activeGame == Game::Word && game.word.solved)
                       ? Glow::WordSolved
                   : (activeGame == Game::Run && newBest) ? Glow::Win
                                                          : Glow::None,
                   now);
}
bool game_button(int key, uint64_t now) {
  if (scene == Scene::Pause) {
    move_selection(key, 2);
    if (key == board::B || key == board::Start ||
        (key == board::A && !selected)) {
      go(pauseReturn);
      resume_game(now);
    } else if (key == board::A)
      go(Scene::Games, int(activeGame) - 1);
    return true;
  }
  if ((scene == Scene::Word || scene == Scene::Run || scene == Scene::Bomb) &&
      key == board::Start) {
    pauseReturn = scene;
    pause_game(now);
    go(Scene::Pause);
    return true;
  }
  if (scene == Scene::Games) {
    move_selection(key, 3);
    if (key == board::A && !wantTicket)
      open_game(Game(selected + 1), now);
    return key != board::B && key != board::Start;
  }
  if (scene == Scene::Word) {
    game.word.input(key, now);
    return true;
  }
  if (scene == Scene::Run) {
    game.run.input(key);
    return true;
  }
  if (scene == Scene::Bomb) {
    if (session.mode == Mode::Solo && key == board::A) {
      // Explicit preflight offers host wallet, with a safe Solo checkpoint
      // first.
      checkpoint();
      if (!saveOk)
        return true;
      release_game();
      start(true, now);
      if (session.mode == Mode::Host) {
        open_game(Game::Bomb, now);
        game.bomb.create(now);
      }
      return true;
    }
    if (key == board::B &&
        (game.bomb.phase == Bomb::Off || game.bomb.phase == Bomb::Lobby ||
         game.bomb.phase == Bomb::Cancelled)) {
      go(Scene::Games, 2);
      return true;
    }
    game.bomb.input(key, now);
    return true;
  }
  if (scene == Scene::Result) {
    if (key == board::B)
      go(Scene::Games, std::max(0, int(activeGame) - 1));
    if (key == board::A && rewardSettled) {
      Game next = activeGame;
      go(Scene::Games, int(next) - 1);
      open_game(next, now);
    }
    return true;
  }
  return false;
}
void game_tick(uint64_t now) {
  if (cancelAfter && !session.pending) {
    cancelAfter = false;
    if (session.player >= 0 && session.world.p[session.player].ticket) {
      Request r{};
      r.op = Op::CancelGame;
      session.act(r, now);
    }
  }
  if (wantTicket && !session.pending && scene != Scene::Leave) {
    wantTicket = false;
    if (scene == Scene::Games && session.last.code == Error::Ok &&
        session.last.ticket) {
      activeGame = requestedGame;
      requestedGame = Game::None;
      pausedAt = 0;
      gameAt = now;
      rewardSettled = false;
      claimStage = 0;
      if (activeGame == Game::Word) {
        new (&game.word) MemeWord{};
        game.word.start(session.last.ticket);
        go(Scene::Word);
      } else if (activeGame == Game::Run) {
        new (&game.run) Runner{};
        game.run.start(session.last.ticket);
        previousBest = session.world.p[session.player].runBest;
        go(Scene::Run);
      }
    }
  }
  if (activeGame == Game::Word && scene == Scene::Word) {
    auto &g = game.word;
    int key = (held & (1 << board::Up)) && !(held & (1 << board::Down))
                  ? board::Up
              : (held & (1 << board::Down)) && !(held & (1 << board::Up))
                  ? board::Down
                  : -1;
    if (key != repeatKey) {
      repeatKey = key;
      repeatStart = now;
      repeatAt = now + 400;
    } else if (key >= 0 && now >= repeatAt) {
      g.input(key, now);
      repeatAt = now + (now - repeatStart >= 800 ? 60 : 120);
      dirty = true;
    }
    if (g.revealAt && now < g.revealAt + 1200) {
      dirty = true;
      if (g.solved && feedback.effect != Glow::WordSolved)
        feedback.trigger(Glow::WordSolved, g.revealAt);
    }
    if (g.done && now >= g.revealAt + 1200) {
      newBest = false;
      show_result(now, g.solved ? "SOLVED!" : "NICE TRY");
    }
  }
  if (activeGame == Game::Run && scene == Scene::Run) {
    unsigned steps = 0;
    auto &g = game.run;
    while (now >= gameAt + 20 && steps++ < 4 && !g.done) {
      auto hearts = g.hearts;
      auto combo = g.combo;
      gameAt += 20;
      g.step();
      if (hearts != g.hearts)
        feedback.trigger(Glow::Error, now);
      else if (combo != g.combo)
        feedback.trigger(Glow::Reward, now);
    }
    dirty = true;
    if (g.done) {
      newBest = g.score > previousBest;
      show_result(now, g.hearts ? "RUN COMPLETE" : "RUN OVER");
      if (g.overflow) {
        claimStage = 0;
        rewardSettled = true;
        resultTitle = "PRACTICE - TRACE FULL";
      }
    }
  }
  if (activeGame == Game::Bomb) {
    game.bomb.tick(now);
    dirty |= game.bomb.changed;
    game.bomb.changed = false;
    if (game.bomb.phase == Bomb::Live || game.bomb.phase == Bomb::Countdown)
      dirty = true;
    if (game.bomb.phase == Bomb::Over && scene != Scene::Result &&
        scene != Scene::Leave) {
      resultTitle = game.bomb.mine() ? "YOU GOT RUGGED" : "YOU SURVIVED";
      resultAt = now;
      int me = game.bomb.member(session.player);
      rewardValue = me >= 0 ? game.bomb.members[me].reward : 0;
      rewardSettled = true;
      claimStage = 0;
      go(Scene::Result);
      feedback.trigger(game.bomb.mine() ? Glow::Rug : Glow::Reward, now);
    }
  }
  if (claimStage && scene == Scene::Result && !session.pending &&
      now >= claimRetryAt) {
    if (claimObserved != session.completed) {
      claimObserved = session.completed;
      if (session.completedOp == Op::RunChunk &&
          session.last.code == Error::Wait) {
        chunkPending = 0;
        claimRetryAt = now + 500;
        return;
      }
      if (session.completedOp == Op::RunChunk &&
          session.last.code == Error::Ok) {
        uploadOffset += chunkPending;
        chunkPending = 0;
      }
      if (session.last.code != Error::Ok) {
        claimStage = 0;
        rewardSettled = true;
        resultTitle = "REWARD NOT ACCEPTED";
        return;
      }
      if (session.completedOp == Op::Claim) {
        rewardValue = session.last.value;
        rewardSettled = true;
        claimStage = 0;
        feedback.trigger(Glow::Reward, now);
        return;
      }
    }
    Request r{};
    if (activeGame == Game::Run && uploadOffset < game.run.traceCount * 3) {
      auto len = std::min(180, int(game.run.traceCount * 3 - uploadOffset));
      r.op = Op::RunChunk;
      r.coin = 2;
      r.amount = uploadOffset;
      r.symbol[0] = len;
      r.symbol[1] = len >> 8;
      std::memcpy(r.proof, game.run.trace + uploadOffset, len);
      if (session.act(r, now))
        chunkPending = len;
    } else {
      r.op = Op::Claim;
      r.coin = uint8_t(activeGame);
      if (activeGame == Game::Word) {
        r.amount = game.word.row;
        for (int i = 0; i < game.word.row; i++)
          std::memcpy(r.proof + 5 * i, game.word.guesses[i], 5);
      } else {
        r.amount = game.run.tick;
        Writer w{r.proof, sizeof r.proof};
        w.u32(game.run.score);
        w.u16(game.run.traceCount * 3);
      }
      session.act(r, now);
    }
  }
}
void ui_text(board::Screen &f, int x, int y, uint16_t color, int scale,
             const char *pattern, ...) {
  char s[52];
  va_list args;
  va_start(args, pattern);
  vsnprintf(s, sizeof s, pattern, args);
  va_end(args);
  f.text(x, y, s, color, scale);
}
void game_header(board::Screen &f, const char *title, const char *hint) {
  f.custom = true;
  f.text(14, 10, title, board::Ink, 2);
  f.box(14, 32, 292, 1, board::Rule);
  f.text(14, 226, hint, board::Muted);
}
void player_name(char out[16], unsigned id) {
  if (id >= session.world.players) {
    std::snprintf(out, 16, "UNKNOWN");
    return;
  }
  auto &mac = session.world.p[id].mac;
  std::snprintf(out, 16, "BADGE %02X%02X", mac.b[4], mac.b[5]);
}
bool game_draw(board::Screen &f, uint64_t now) {
  using namespace board;
  auto *p = session.player >= 0 ? &session.world.p[session.player] : nullptr;
  if (scene == Scene::Coin && p && coinId > 0 &&
      coinId <= session.world.coins) {
    auto &c = session.world.c[coinId - 1];
    static uint32_t prices[MaxCoins]{}, previous[MaxCoins]{};
    static uint64_t priceAt[MaxCoins]{};
    unsigned index = coinId - 1;
    uint32_t price = 100 + 5 * c.supply;
    if (!prices[index])
      prices[index] = previous[index] = price;
    if (prices[index] != price) {
      previous[index] = prices[index];
      prices[index] = price;
      priceAt[index] = now;
    }
    bool moving = priceAt[index] && now < priceAt[index] + 500;
    int64_t shown = moving ? int64_t(previous[index]) +
                                 (int64_t(price) - previous[index]) *
                                     int64_t(now - priceAt[index]) / 500
                           : price;
    game_header(f, c.symbol, "UP/DOWN OPTIONS   A SELECT   B SELL");
    if (c.rugged) {
      f.box(235, 8, 72, 19, Red);
      f.text(241, 14, "RUGGED", Paper);
    }
    f.text(18, 45, "TOKEN PRICE", Muted);
    ui_text(f, 18, 61, Gold, 3, "%lld.%03lld SOL", (long long)shown / 1000,
            (long long)shown % 1000);
    if (moving) {
      f.text(250, 65, price > previous[index] ? "UP" : "DOWN",
             price > previous[index] ? Green : Red);
      dirty = true;
    }
    ui_text(f, 18, 99, Muted, 1, "YOU OWN %u TOKENS", owned(*p, coinId));
    ui_text(f, 18, 117, Muted, 1, "SUPPLY %u / MEME POWER %u", c.supply,
            c.meme);
    bool committed =
        session.last.coin == coinId && session.last.code == Error::Ok &&
        now < feedback.until &&
        ((session.completedOp == Op::Buy && feedback.effect == Glow::Buy) ||
         (session.completedOp == Op::Sell && feedback.effect == Glow::Sell));
    if (committed) {
      bool buy = session.completedOp == Op::Buy;
      ui_text(f, 215, 99, buy ? Green : Red, 2, "%s%u", buy ? "+" : "-",
              session.last.amount);
      f.box(12, 92, 296, 22, buy ? Green : Red, true);
      ui_text(f, 18, 205, Gold, 1, "%s %u FOR %lu.%03lu SOL",
              buy ? "PURCHASED" : "SOLD", session.last.amount,
              (unsigned long)session.last.value / 1000,
              (unsigned long)session.last.value % 1000);
    } else
      f.text(18, 205, session.status, Muted);
    f.box(16, 140, 137, 29, Surface);
    f.box(166, 140, 137, 29, Surface);
    f.box(selected == 0 ? 16 : 166, 140, 137, 29, selected < 2 ? Cyan : Rule,
          true);
    f.text(31, 148, c.rugged ? "CLOSED" : "BUY", c.rugged ? Disabled : Green,
           2);
    f.text(183, 148, "SELL", Red, 2);
    bool creator = c.creator == session.player;
    if (creator)
      f.text(18, 180, selected == 2 ? "> CREATOR CONTROLS" : "CREATOR CONTROLS",
             selected == 2 ? Cyan : Muted);
    f.text(250, 180, selected == (creator ? 3 : 2) ? "> BACK" : "BACK",
           selected == (creator ? 3 : 2) ? Cyan : Muted);
    return true;
  }
  if (scene == Scene::Portfolio && p) {
    game_header(f, "PORTFOLIO", "UP/DOWN CHOOSE   A TRADE   B BACK");
    for (int i = 0; i < 4; i++) {
      auto &h = p->holdings[i];
      int y = 40 + i * 42;
      f.box(12, y, 296, 38, Surface);
      f.box(12, y, 296, 38, i == selected ? Cyan : Rule, true);
      if (!h.coin) {
        f.text(24, y + 12, "EMPTY SLOT", Muted);
        continue;
      }
      auto &c = session.world.c[h.coin - 1];
      f.text(23, y + 6, c.symbol, Ink, 2);
      ui_text(f, 23, y + 24, Muted, 1, "%u TOKENS", h.quantity);
      auto gross = cost(c.supply - h.quantity, h.quantity);
      int64_t value = gross - (c.rugged ? 0 : (gross + 49) / 50);
      ui_text(f, 156, y + 6, Gold, 1, "%lld.%03lld SOL",
              (long long)value / 1000, (long long)value % 1000);
      if (h.basisKnown && h.basis) {
        int64_t delta = value - h.basis;
        auto pct = delta * 1000 / h.basis;
        ui_text(f, 156, y + 23, delta >= 0 ? Green : Red, 1, "%s %s%lld.%lld%%",
                delta >= 0 ? "UP" : "DOWN", delta >= 0 ? "+" : "-",
                (long long)std::abs(pct) / 10, (long long)std::abs(pct) % 10);
      } else
        f.text(156, y + 23, "BASIS UNAVAILABLE", Muted);
    }
    ui_text(f, 16, 213, Gold, 1, "WALLET %lu.%03lu SOL",
            (unsigned long)p->balance / 1000, (unsigned long)p->balance % 1000);
    return true;
  }
  if (scene == Scene::Games) {
    game_header(f, "GAMES", "UP/DOWN CHOOSE   A PLAY   B DASHBOARD");
    static const char *names[] = {"MEMEWORD", "RUG RUN", "RUG BOMB"};
    static const char *hints[] = {"FIVE LETTERS. SIX GUESSES.",
                                  "DODGE. JUMP. CHASE YOUR BEST.",
                                  "2-6 BADGES. PASS THE RUG."};
    for (int i = 0; i < 3; i++) {
      int y = 42 + i * 57;
      f.box(12, y, 296, 51, Surface);
      f.box(12, y, 296, 51, selected == i ? Cyan : Rule, true);
      if (i == 0) {
        for (int k = 0; k < 3; k++)
          f.box(23 + k * 8, y + 15, 6, 8,
                k == 0   ? Green
                : k == 1 ? Amber
                         : Disabled);
      }
      if (i == 1) {
        f.box(28, y + 10, 9, 20, Cyan);
        f.box(25, y + 24, 15, 6, Gold);
      }
      if (i == 2) {
        f.box(25, y + 16, 18, 17, Red);
        f.box(37, y + 10, 3, 8, Gold);
      }
      f.text(56, y + 6, names[i], Ink, 2);
      f.text(56, y + 25, hints[i], Muted);
      if (i == 0)
        ui_text(f, 56, y + 37, Gold, 1, "UP TO 10 SOL / BEST %u",
                p ? p->wordBest : 0);
      if (i == 1)
        ui_text(f, 56, y + 37, Gold, 1, "BEST %lu / UP TO 5 SOL",
                (unsigned long)(p ? p->runBest : 0));
      if (i == 2)
        f.text(56, y + 37, "SURVIVE +2 SOL / PASS BONUS", Blue);
    }
    if (wantTicket || session.pending)
      f.text(14, 216, "WAITING FOR HOST...", Cyan);
    else
      f.text(14, 216, session.status, Muted);
    return true;
  }
  if (scene == Scene::Word && activeGame == Game::Word) {
    auto &g = game.word;
    game_header(f, "MEMEWORD", "L/R CURSOR U/D LETTER A GUESS B CLEAR");
    for (int row = 0; row < 6; row++)
      for (int col = 0; col < 5; col++) {
        int x = 70 + col * 36, y = 39 + row * 27, h = 24;
        bool submitted = row < g.row;
        auto mark = g.marks[row][col];
        uint16_t color = Surface;
        bool reveal =
            submitted &&
            !(row == g.row - 1 && now < g.revealAt + uint64_t(col + 1) * 120);
        if (reveal)
          color = mark == 2 ? Green : mark == 1 ? Amber : Disabled;
        if (submitted && row == g.row - 1 && now >= g.revealAt + col * 120 &&
            now < g.revealAt + (col + 1) * 120) {
          int t = int(now - g.revealAt - col * 120);
          h = std::max(2, std::abs(t - 60) * 24 / 60);
          y += (24 - h) / 2;
        }
        if (g.solved && now >= g.revealAt + 700 + col * 80 &&
            now < g.revealAt + 780 + col * 80)
          y -= 3;
        f.box(x, y, 30, h, color);
        if (row == g.row)
          f.box(x - 1, y - 1, 32, h + 2, col == g.cursor ? Cyan : Rule, true);
        char letter[2] = {g.guesses[row][col] ? g.guesses[row][col] : '-', 0};
        if (h > 16)
          f.text(x + 9, y + 4, letter, reveal && mark ? Paper : Ink, 2);
        if (reveal && mark == 2) {
          f.box(x + 23, y + 19, 2, 3, Paper);
          f.box(x + 25, y + 17, 2, 3, Paper);
        } else if (reveal && mark == 1)
          f.box(x + 24, y + 19, 3, 3, Paper);
      }
    f.text(14, 207, g.message, g.done && g.solved ? Green : Muted);
    return true;
  }
  if (scene == Scene::Run && activeGame == Game::Run) {
    auto &g = game.run;
    game_header(f, "RUG RUN", "LEFT/RIGHT LANES   A JUMP   START PAUSE");
    ui_text(f, 166, 12, Gold, 2, "%lu", (unsigned long)g.score);
    int shake = g.immune > 32 ? int(g.tick % 3) - 1 : 0;
    for (int l = 0; l < 3; l++) {
      int x = 20 + l * 96;
      f.box(x, 38, 88, 174, Surface);
      for (int j = 0; j < 5; j++)
        f.box(x + 43, 40 + (j * 40) % 168, 2, 10, Rule);
    }
    for (auto &o : g.objects)
      if (o.active && o.y >= 36 && o.y < 215) {
        int x = 64 + 96 * o.lane + shake, y = o.y;
        if (o.kind == 1) {
          f.box(x - 17, y - 6, 34, 12, Red);
          f.box(x - 19, y + 6, 38, 2, Amber);
        } else if (o.kind == 2) {
          f.box(x - 9, y - 15, 18, 26, Red);
          f.box(x - 1, y - 22, 2, 40, Red);
        } else {
          uint16_t c = o.kind == 3   ? Gold
                       : o.kind == 4 ? Cyan
                       : o.kind == 5 ? Purple
                                     : Green;
          f.box(x - 7, y - 7, 14, 14, c, true);
          f.text(x - 3, y - 3,
                 o.kind == 3   ? "$"
                 : o.kind == 4 ? "S"
                 : o.kind == 5 ? "M"
                               : "R",
                 c);
        }
      }
    int lift = g.jump ? (25 - std::abs(int(g.jump) * 2 - 25)) * 1 : 0;
    int x = g.x + shake, y = 181 - lift;
    f.box(g.x - 12, 198, 24, 3, Disabled);
    if (!g.immune || g.tick % 4 < 2) {
      f.box(x - 10, y, 20, 11, g.rocket ? Gold : Ink);
      f.box(x + 3, y - 9, 10, 16, Ink);
      f.box(x + 10, y - 4, 9, 4, Gold);
      f.box(x + 8, y - 7, 2, 2, Paper);
      f.box(x - 7, y + 11, 4, 5, Gold);
      f.box(x + 3, y + 11, 4, 5, Gold);
      if (g.shield)
        f.box(x - 14, y - 13, 38, 32, Cyan, true);
    }
    for (auto &v : g.particles)
      if (v.life)
        f.box(v.x, v.y, 2, 2, Gold);
    for (int i = 0; i < 3; i++) {
      uint16_t c = i < g.hearts ? Red : Disabled;
      f.box(15 + i * 16, 36, 4, 4, c);
      f.box(21 + i * 16, 36, 4, 4, c);
      f.box(16 + i * 16, 39, 8, 3, c);
      f.box(19 + i * 16, 42, 2, 2, c);
    }
    ui_text(f, 240, 37, Cyan, 1, "X%u COMBO", g.combo);
    if (g.immune > 32)
      f.box(x - 15, y - 14, 39, 34, Red, true);
    return true;
  }
  if (scene == Scene::Pause) {
    game_header(f, activeGame == Game::Bomb ? "ROUND CONTINUES" : "PAUSED",
                "A SELECT   B RESUME");
    f.text(24, 64,
           activeGame == Game::Bomb ? "THE RUG TIMER KEEPS RUNNING"
                                    : "TAKE A BREATH",
           Muted);
    f.box(18, 101 + selected * 40, 284, 31, Rule);
    f.box(18, 101 + selected * 40, 284, 31, Cyan, true);
    f.text(30, 109, "RESUME", Ink, 2);
    f.text(30, 149, "QUIT TO GAMES", Ink, 2);
    return true;
  }
  if (scene == Scene::Bomb && activeGame == Game::Bomb) {
    auto &g = game.bomb;
    game_header(f, "RUG BOMB", "A SELECT/PASS   START OPTIONS");
    if (session.mode == Mode::Solo) {
      f.text(20, 65, "PLAY WITH 2-6 NEARBY BADGES", Ink);
      f.text(20, 90, "A SAVE SOLO AND OPEN HOST MARKET", Cyan);
      f.text(20, 116, "HOST WALLET IS SEPARATE", Gold);
      f.text(20, 145, "FRIENDS JOIN YOUR HOST CODE", Muted);
      f.text(20, 180, "B GAMES", Muted);
      return true;
    }
    if (g.phase == Bomb::Off) {
      f.text(20, 72, g.message, Cyan, 1);
      f.text(20, 110, "HOST OPENS GAME; FRIENDS JOIN", Muted);
      return true;
    }
    if (g.phase == Bomb::Lobby) {
      ui_text(f, 18, 41, Cyan, 1, "HOST %02X%02X / %u OF 6", session.host.b[4],
              session.host.b[5], g.count);
      for (int i = 0; i < g.count; i++) {
        char name[16];
        player_name(name, g.members[i].player);
        ui_text(f, 22, 63 + i * 21, g.members[i].ready ? Green : Muted, 1,
                "%s   %s", name, g.members[i].ready ? "READY" : "WAITING");
      }
      f.text(18, 203, g.message, Cyan);
      return true;
    }
    if (g.phase == Bomb::Cancelled) {
      f.text(20, 90, "ROUND CANCELLED", Red, 2);
      f.text(20, 123, g.message, Muted);
      return true;
    }
    if (g.phase == Bomb::Countdown) {
      f.text(24, 63, "GET READY", Cyan, 2);
      ui_text(f, 140, 100, Gold, 6, "%u",
              unsigned(3 - std::min<uint64_t>(2, (now - g.phaseAt) / 1000)));
      return true;
    }
    if (g.target == session.player) {
      static const char *keys[] = {"A",    "B",     "HOME", "DOWN",
                                   "LEFT", "RIGHT", "UP"};
      f.text(28, 55, "INCOMING RUG!", Red, 2);
      f.text(28, 92, g.responded ? "WAIT FOR HOST COMMIT" : "PRESS THE BUTTON",
             Muted);
      f.text(78, 124, g.key < 7 ? keys[g.key] : "?", Cyan, 4);
      return true;
    }
    if (g.selecting) {
      f.text(18, 44, "PASS TO:", Cyan, 2);
      for (int i = 0; i < g.count; i++) {
        char name[16];
        player_name(name, g.members[i].player);
        if (i == g.selection)
          f.box(18, 67 + i * 22, 282, 20, Rule);
        ui_text(f, 25, 73 + i * 22,
                g.members[i].player == g.holder ? Muted : Ink, 1, "%s %s", name,
                g.members[i].player == g.holder ? "(YOU)" : "");
      }
      f.text(18, 211, g.message, Muted);
      return true;
    }
    f.box(133, 56, 54, 44, g.mine() ? Red : Blue);
    f.box(166, 44, 5, 16, Gold);
    f.box(174, 42, 10, 3, Gold);
    f.text(28, 120, g.mine() ? "YOU HAVE THE RUG" : "SAFE... FOR NOW",
           g.mine() ? Red : Blue, 2);
    f.text(80, 153,
           g.tension == 2   ? "OH NO!"
           : g.tension == 1 ? "GETTING HOT"
                            : "CALM",
           g.tension == 2 ? Red : Gold, 2);
    f.text(36, 192,
           g.mine() ? (g.target != 255 ? "PASS PENDING - STILL YOUR RUG"
                                       : "A PASS IT!")
                    : "WAIT FOR AN INCOMING CHALLENGE",
           Cyan);
    f.text(18, 211, g.message, Muted);
    return true;
  }
  if (scene == Scene::Result) {
    game_header(f, resultTitle, "A PLAY AGAIN   B GAMES");
    const char *name = activeGame == Game::Word  ? "MEMEWORD"
                       : activeGame == Game::Run ? "RUG RUN"
                                                 : "RUG BOMB";
    f.text(18, 44, name, Cyan, 2);
    if (activeGame == Game::Word)
      ui_text(f, 18, 71, Ink, 2, "%s / %u OF 6", game.word.answer,
              game.word.row);
    if (activeGame == Game::Run) {
      ui_text(f, 18, 70, Ink, 2, "SCORE %lu", (unsigned long)game.run.score);
      ui_text(f, 18, 93, newBest ? Green : Muted, 1, "%s %lu",
              newBest ? "NEW HIGH SCORE!" : "BEST",
              (unsigned long)std::max(previousBest, game.run.score));
    }
    if (activeGame == Game::Bomb) {
      char name[16];
      player_name(name, game.bomb.holder);
      ui_text(f, 18, 73, Red, 1, "%s GOT RUGGED", name);
    }
    f.text(18, 116, "REWARD", Muted);
    if (!rewardSettled)
      f.text(18, 139, "VERIFYING WITH HOST...", Cyan, 2);
    else
      ui_text(f, 18, 137, Gold, 3, "+%lu.%03lu SOL",
              (unsigned long)rewardValue / 1000,
              (unsigned long)rewardValue % 1000);
    if (rewardSettled && !rewardValue)
      f.text(18, 169, "PRACTICE / REWARD LIMIT", Muted);
    if (p && activeGame == Game::Bomb && p->bombRound < game.bomb.round)
      f.text(18, 190, "BALANCE SYNCING WITH HOST...", Cyan);
    else if (p)
      ui_text(f, 18, 190, Gold, 1, "NEW BALANCE %lu.%03lu SOL",
              (unsigned long)p->balance / 1000,
              (unsigned long)p->balance % 1000);
    if (activeGame == Game::Bomb && game.bomb.mine() && now < resultAt + 700) {
      f.box(1, 1, 318, 238, Red, true);
      f.box(6, 6, 308, 228, Red, true);
    }
    return true;
  }
  return false;
}
