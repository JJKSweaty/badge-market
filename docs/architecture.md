# Badge Market — implementation plan

Updated 2026-09-20. **Status: native v0.3.0 implementation delivered; physical
qualification continues in [the verification record](../firmware/VERIFICATION.md).** This
replaces the earlier implementation contract and its proposed game lineup.
The release target is exactly three polished games: **MEMEWORD**, **RUG RUN**,
and **RUG BOMB**. Reaction Trader, Tilt Vault, and Badge Duel leave the release
menus, runtime, reward endpoints, and player instructions when this work ships.
They are not additional launch games.

The target is the existing **native C++17 ESP-IDF firmware** in `firmware/`.
It already provides ESP-NOW, integer market accounting, saves, buttons, a
320×240 primitive renderer, and six WS2812 LEDs. It does not run Lua. Interpret
“only the selected game in Lua/RAM” as one active native game's resources and
state; compiled code and constant assets remain in flash. Adding Lua is not
required. The retained stock Lua/BLE app cannot meet the ESP-NOW and complete
module-unloading requirements; [stock-runtime.md](stock-runtime.md) describes
that historical target.

All displayed SOL is **fake game money**: `1 SOL = 1,000 GAME_SOL`, with checked
integer arithmetic internally. No real cryptocurrency, wallet integration,
payments, external service, phone, or internet is part of this plan.

## 1. Product contract and current baseline

| Game | Motivation | Launch experience |
| --- | --- | --- |
| MEMEWORD | Thinking, puzzle skill, periodic replay | Five letters, six guesses, directional entry, satisfying reveals |
| RUG RUN | Reflexes, high scores, arcade replay | Three lanes, jumps, fair procedural obstacles, short escalating runs |
| RUG BOMB | Nearby social play and chaos | 2–6 real badges, interactive passing, hidden host-controlled explosion |

Each game earns spendable GAME_SOL, shows the credited reward and wallet, and
returns quickly to Games or another round. Replayability, readable outcomes,
responsive input, and consistent feedback are release requirements.

Reuse the current economy: 25 SOL starting balance, 2 SOL launch cost, four
holdings/player, 12 market players, 16 lifetime coins, 10,000-token supply cap,
bonding curve and 2% fees. Preserve host authority, sequential/idempotent
transactions, separate Solo/Host saves, and protected curve reserves. Rugs
withdraw fee treasuries; holders can still sell. The reward policy below
replaces the old Reaction-specific ticket, proof, cooldown, and 6 SOL epoch cap.

| Existing files | Planned change |
| --- | --- |
| `firmware/main/main.cpp` | Replace Reaction/Maze/Duel scenes and globals; add Games, lifecycle, pause/results, richer trade UI |
| `firmware/core/play.{hpp,cpp}`, `bomb.{hpp,cpp}` | Bounded game rules and multiplayer authority; old native games removed |
| `firmware/core/market.{hpp,cpp}` | Typed game tickets, reward admission, saved caps/receipts/statistics, snapshot migration |
| `firmware/core/session.{hpp,cpp}` | Versioned game messages, bounded proofs, RUG BOMB commits/recovery; preserve market RPC behavior |
| `firmware/main/display.hpp`, `board.{hpp,cpp}` | Shared cards/tiles/sprites, dirty regions, RGB565 palette, timed input repeat |
| `firmware/core/feedback.hpp` | Shared animation/LED language driven by committed outcomes |
| `firmware/main/storage.cpp` | Versioned A/B saves scheduled outside live gameplay |
| `firmware/tests/`, `tools/test_native.sh` | Rules, actual-controller, rendering, loss/replay, and resource verification |

The v0.2.4 baseline redrew full screens and gated drawing at 80 ms. v0.3.0
replaces that with command damage tracking and pipelined eight-row DMA stripes. Saves and Session transfer buffers currently
cap at 2 KiB; size new fields explicitly. Results in
[firmware/VERIFICATION.md](../firmware/VERIFICATION.md) establish the old build's
baseline, not performance or completion of these new games.

## 2. Shared runtime, memory, and scheduling

Use `enter → update/input → render → exit`, with one tagged, reusable game arena.
Construct only the selected game's state. Exit destroys that state, clears
references/callbacks/input repeats, releases scratch storage, and stops game
timers, effects, and subscriptions. The arena may remain reserved for reuse,
but contains no live resources from an exited game. Shared market/radio services
and compact durable statistics are not game scenes.

Code, fonts, tiny pixel sprites, dictionary data, and three game descriptors
stay in flash. Never copy all games' resources into RAM. A future custom Lua
port must load only one game module/environment, remove module/registry
references on exit, and collect during transition; the stock retained-module
cache does not satisfy this requirement.

These initial bounds are **engineering targets to measure**, not claims that
the new build fits or meets timing:

| Resource | Initial bound |
| --- | --- |
| Active game arena | 8 KiB maximum; measure each game and union size |
| Shared display | Two 320×8×2 DMA stripes = 10,240 bytes; no framebuffer |
| Renderer | 128 primitive commands, bounded fingerprints, and a 30-stripe dirty mask |
| Effects | 12 particles, four small transitions/effects, one prioritized LED effect |
| RUG RUN world | Two 16-column chunks, 12 obstacles, 16 pickups; fixed pools |
| RUG BOMB | Six participants, one outstanding pass challenge, bounded retry records |
| Dictionary | Initially about 256 answers and 1,024 valid guesses, five bytes/word |
| System reserve | At least 32 KiB free internal heap at worst-case load; track largest block too |

All objects, commands, proof buffers, and effects have hard bounds and explicit
overflow behavior. No obstacle `malloc/free`, per-frame strings, unbounded
queues, complicated tile objects, large images, GIFs, decoded frame sequences,
or heavy per-pixel transparency. Reuse native text, tiles, buttons, cards,
sprites, particles, score, currency, and result components.

Scan/debounce input independently of painting, targeting a 10 ms service
interval. RUG RUN uses a fixed simulation step (initially 20 ms) and interpolated
rendering; cap catch-up work and record overruns. Target 30 FPS during motion,
p95 frame work within 33 ms, and input-to-visible response within 50 ms on
hardware. Log worst cases as well as percentiles. Static scenes render only
changed regions. Shed decorative effects first if over budget; never skip
collision checks, input handling, or authoritative deadlines.

Radio callbacks only copy validated-length packets into bounded queues. The
main task owns game state, handles a bounded packet batch, and keeps market
requests working while the host plays. Initialize resources/radio before a
round. Single-player games in Solo leave radio off; in a market they reuse the
existing connection, with no per-frame game traffic.

No file loads, flash writes, network initialization, or expensive allocation
during live gameplay. Memory-mapped constant dictionary access is allowed.
Defer autosaves to pause/result/lobby/transition safe points; service due saves
before another round. Explicit Host/Solo market exit still saves successfully
before closing. Show dirty/save-failure status and do not promise the old
30-second checkpoint interval during a long puzzle. A storage worker alone
is not proof against flash stalls: measure before adopting that alternative.

## 3. Games screen and navigation

Replace Minigames and the separate Duels entry with **Games**. Three small
flash-backed records use one reusable card widget, vertical scroll, deep navy
background, lighter cards, and cyan selected border. Use tiny native-drawn or
pixel-art tile/rocket/bomb icons, not large assets or required emoji glyphs.

The selected card expands to description, best result, reward policy, and Play.
Other cards stay compact. Show MEMEWORD availability and best guess count,
RUG RUN high score, and RUG BOMB “2–6 players” with nearby lobby status. Derive
reward copy from configuration: the initial MEMEWORD maximum is **10 SOL**,
so do not retain the illustrative “up to 8 SOL” card text.

Up/Down selects; A opens; B returns to the dashboard. Preserve selection when
returning. START pauses MEMEWORD/RUG RUN and opens **Resume / Quit to Games**,
defaulting to Resume; B cancels. HOME opens the market leave prompt. Consume
the entering button edge so it cannot also confirm in the next screen.
Single-player pause/leave overlays freeze the gameplay clock and restore it
without catch-up on cancel; the market authority clock continues. Exclude pause
time from Run simulation/proofs and clear held-input repeats when resuming.

RUG BOMB cannot pause the host timer. START opens an overlay explaining that
the round continues, with Return / Leave game; HOME retains market-leave
behavior. Quitting a live game earns no participation reward. Returning from
Games does not disconnect the market. Market exit follows existing save/failure/
retry rules and cancels game networking before releasing resources.

## 4. MEMEWORD

### Rules, controls, and dictionary

Guess a recognizable five-letter word in six valid attempts, without a keyboard.

| Input | Behavior |
| --- | --- |
| LEFT / RIGHT | Move cursor one position, clamped to the five-letter row |
| UP / DOWN | Cycle selected letter forward/backward; alphabet wraps Z↔A |
| A | Submit a complete, dictionary-valid guess |
| B | Clear selected letter to blank |
| START | Pause / quit confirmation |

Start letters blank; first Up enters A, first Down enters Z. Apply one change
immediately, repeat after about 400 ms at 120 ms intervals, then after 800 ms
at 60 ms intervals. Reset acceleration on release or direction change; opposing
directions cancel. Only Up/Down auto-repeat. Bright cyan outlines the selected
letter; the current row also has its own focus marker.

Incomplete/unknown words show brief inline feedback without consuming an
attempt. Score green positions first, then consume remaining answer-letter
counts for amber matches. Duplicate letters cannot receive more matches than
the answer contains. A compact known-letter strip is optional if legible.

Generate sorted flash arrays for **answer words** and **valid guess words**,
with every answer valid as a guess. Start near 256 curated answers and 1,024
guesses; five-byte uppercase entries cost about 6.4 KiB flash before metadata.
Native binary search compares bytes directly. No dictionary loaded into a Lua
table, and no general English corpus. Five-bit encoding is optional only if
measured flash savings justify added lookup complexity.

Recognizable English dominates. Small TECH, MEMES, ANIMALS, and HACKATHON sets
may add occasional labelled themes. Review spelling, duplicates, familiarity,
and dictionary versioning as content work, not just a build step.

For the offline hackathon build, offer a **round puzzle** every two persisted
120-second authority-uptime epochs, derived from market identity, a monotonic
puzzle counter, and dictionary version. Solo uses its separate saved authority.
Do not label this “today's word” without a reliable date source. Future daily
mode can reuse the puzzle-ID contract. Pin an entered puzzle until it finishes;
no new rewarded puzzle ticket until the previous one resolves or expires.

### Display and outcomes

Fit all six rows on the 320×240 panel with title and concise input hints.
Prototype five 28-pixel tiles with small gaps and six compact rows; verify
font/layout physically. Keep `uint8_t guesses[6][5]` and fixed-byte or two-bit
feedback per tile. Cache text and redraw changed tiles only.

| Tile state | Appearance and non-color cue |
| --- | --- |
| Unentered | Surface fill, subtle border, blank/underscore |
| Current letter | Cyan outline and cursor marker |
| Correct position | Green fill and small check mark |
| Present elsewhere | Amber fill and dot/alternate marker |
| Absent | Dim dark-gray tile and distinct absent mark |

Reveal one tile every 100–140 ms, totaling 500–700 ms, using height contraction,
color swap, and expansion. No 3D transforms. Navigation/pause stay responsive;
suppress duplicate submits. On solve, sequential tile pops and a green→gold
celebration across all six LEDs lead to **SOLVED! / GOOSE / 3 OF 6 / +6.50 SOL**.
Keep the complete solve reveal/celebration under about 1.5 seconds. Six failed
valid guesses show **NICE TRY / WORD: CRANE / +0.75 SOL** in the shared result.

### Initial rewards

| Outcome | GAME_SOL | Displayed SOL |
| --- | ---: | ---: |
| Solve in 1 | 10,000 | 10.00 |
| Solve in 2 | 8,000 | 8.00 |
| Solve in 3 | 6,500 | 6.50 |
| Solve in 4 | 5,000 | 5.00 |
| Solve in 5 | 3,500 | 3.50 |
| Solve in 6 | 2,000 | 2.00 |
| Six guesses, unsolved | 750 | 0.75 |
| Replay / abandoned puzzle | 0 | 0.00 |

Main reward, including failure participation, is available once per player per
puzzle ID. Persist a settled-ID high-water mark and one outstanding ticket;
reject old IDs, client-chosen seeds, and reused proofs. Replay says **Practice —
reward already collected**. Best results still encourage play. Tune the table
through playtesting without weakening one-reward-per-puzzle enforcement.

## 5. RUG RUN

### Core loop

Build a 40-second survival run (tunable within 30–45 seconds) through a collapsing
meme market, with a tiny goose, rocket, or coin mascot. Scroll obstacles toward
the player in **three lanes**. LEFT/RIGHT initiate smooth lane changes (initially
120 ms); A jumps (initially 500 ms). Collision position must match visible
motion. Launch without ducking; DOWN duck is optional only if it improves play.

Red candles/rugs are recognizable lane/jump hazards; bears, broken coins, and
scam tokens can share those simple rules as visual variants. Teach lanes/jumps
at low speed, then ramp speed, obstacle frequency, and mixed patterns without
raising object caps. End at zero hearts or time limit with **RUN OVER** or
**RUN COMPLETE**, respectively.

Start with three hearts. Collision removes one, resets combo, flashes red,
shakes the playfield a few pixels, and grants about 800 ms invulnerability.
Overlapping obstacles cannot drain all hearts at once. A shield absorbs one
hit; reset combo and grant brief protection against repeated collision.

Collect consecutive SOL pickups for visible x2/x3/x4 combo, capped at x4;
initially step up every three pickups. Missing a pickup does not reset it;
damage does. Support three simple timer/flag powerups:

- **Shield:** blocks one hit; cyan player outline.
- **Magnet:** attracts nearby SOL pickups for about four seconds.
- **Rocket:** about two seconds of invulnerability and faster scoring; visibly
  warn on expiry and leave a recoverable path.

Integer score comes from distance/survival ticks, pickups, and combo, separately
from spendable GAME_SOL. Use most of the screen for gameplay, with a sparse
score/combo/hearts HUD and no market panels.

### Fair procedural generation and pools

Use a versioned deterministic PRNG and only current/next 16-column chunks.
Discard passed content, promote next to current, and refill from fixed rules
or templates. Pools cap at 12 obstacles, 16 pickups, and 12 shared particles.
Drop decorative effects on saturation; never lose an object required for a
valid route. No full map, per-obstacle allocation, or unbounded entities.

For every region, compute reachable safe lanes using lane-change time, jump
duration/cooldown, scroll speed, collision size, and the previous region's exit
states. An empty lane is insufficient if unreachable. Preserve a valid route
across chunk boundaries and minimum reaction lead time (initial target at least
600 ms at maximum speed). Place extra collectibles on riskier reachable paths.
Mixed lane/jump challenges must pass the same checks. Invalid candidates fall
back to a known-safe pattern. Record seed and generator/rules version for
future fair score comparisons.

### Reward and feedback

Initial reward in GAME_SOL:

```text
raw = min(5000, 200 + floor(score * 5 / 2))
epoch multiplier for completed run 1 / 2 / 3 / 4+: 100% / 50% / 25% / 0%
credited = min(floor(raw * multiplier / 100), remaining Run cap,
               remaining shared cap, wallet headroom)
```

A first 1,840-point run illustrates **+4.80 SOL** before caps. A real loss is a
completed run; quitting is not. Initial Run cap: 6 SOL/player/epoch, maximum
5 SOL/round. Count settled completed runs even if the cap pays zero; practice
can still update high scores.

Pickups create tiny gold bursts, combo steps cyan/gold text pops, damage one
red LED flash, shield a cyan outline. A new best shows **NEW HIGH SCORE!** and
a gold→cyan→green LED sequence. Shared result shows Score, Best, reward, and
new balance. Play Again quickly begins a fresh seeded run.

## 6. RUG BOMB

### Lobby and hidden timer

Ship **one-explosion mode**, with 2–6 actual nearby badges. The market host also
hosts the game, avoiding a second wallet authority or cross-market payouts.
One lobby/round exists per market. Host remains in RUG BOMB while running it;
normal market service continues for nonparticipants. Guests join nearby lobbies.
**Create game** on a non-host explains the switch to the player's own hosted
market first; wallets stay tied to their markets.

From Solo, Create/Join uses preflight that saves Solo before changing mode.
Joining another market's lobby shows its host code and wallet change first;
never silently switch markets. Finish setup/radio initialization before countdown.

Discover using small jittered ESP-NOW beacons. Show **CREATE GAME** or **NEARBY
GAME / JON'S LOBBY / 3 OF 6 / A JOIN**, readiness, and clear full/incompatible/
unavailable states. Optional aliases are bounded to eight supported characters,
with host-code fallback. Disambiguate duplicate names; bind identity to source
MAC, not text. Bound discovery separately from the six-member roster.

Host creates unique session/round IDs, participant IDs, and random seeds.
Freeze the roster when all participants are ready and at least two are present.
No live-round joins. Host-scheduled **3 / 2 / 1 / GO!** assigns one holder;
bounded delivery acknowledgements prevent starting with unreachable players.

Holder sees a large primitive bomb and **YOU HAVE THE RUG / PASS IT!**; others
see **SAFE… FOR NOW**. A shared seed may drive cosmetics, but a separate
**host-private seed** determines the hidden 10–25-second explosion deadline.
Never broadcast that seed/deadline or include it in public snapshots. Send
coarse **CALM / GETTING HOT / OH NO** tension states, with no numeric countdown
or progress bar revealing exact time. Only the host decides explosion time.

### Interactive passing

Holder presses A to open **PASS TO**, selects an eligible participant with
Up/Down, then A requests a pass. Timer continues through selection. Target sees
**INCOMING RUG! / PRESS [button]**, randomly chosen by host from A, B, Up, Down,
Left, Right. Require a fresh edge after display; held buttons, duplicate packets,
and the menu-confirm edge cannot satisfy the challenge.

Allow about 1.25 seconds of visible response time. A small delivery acknowledgement
and bounded transport grace prevent transit from consuming the whole window.
Host bounds total challenge lifetime and validates ID/key/deadline. Wrong key
or timeout fails once. Never accept a response after explosion. Only one pass
challenge is live per round.

Only **host commit** transfers ownership. Before commit the sender remains
holder; afterward both screens show **RUG PASSED**. Failure shows **PASS FAILED**
and retains ownership. Show the committed holder while pending. A two-second
host-enforced lock prevents immediate return to the previous sender, including
in two-player games; visibly explain that brief lock. Bound request frequency
and successful-pass bonuses independently.

### Protocol and authority

Extend the native protocol with a new compatibility version, explicit
little-endian fields, and packets ≤250 bytes. Validate length, source MAC,
market, destination, version, and CRC before mutation. CRC/MAC identity provides
friendly-game protection, not cryptographic anti-cheat.

Applicable messages carry `session_id`, `round_id`, `sequence`, `sender_id`,
`target_id`, and challenge/commit IDs when needed. Game sequences are separate
from economy sequences; renew sessions explicitly before identifier exhaustion.

| Message | Purpose |
| --- | --- |
| `RUG_GAME_CREATE` | Advertise host lobby, version, name, occupancy |
| `RUG_JOIN` | Join/acknowledge roster membership before start |
| `RUG_READY` | Ready state and lobby revision |
| `RUG_START` | Frozen roster, start/countdown revision, holder; no secret deadline |
| `RUG_PASS_REQUEST` | Holder requests an eligible target |
| `RUG_PASS_CHALLENGE` | Host issues unique key challenge with bounded delivery/response handling |
| `RUG_PASS_RESPONSE` | Target reports fresh response for that challenge |
| `RUG_TRANSFER_COMMIT` | Host alone increments ownership revision and announces holder |
| `RUG_EXPLODE` | Host terminal event naming committed holder |
| `RUG_RESULT` | Committed outcomes, rewards, settlement references |
| `RUG_ACK`, `RUG_STATE`, `RUG_LEAVE` | Delivery/recovery, compact state on request, explicit departure |

Host checks membership, ownership, target, cooldown, challenge freshness, and
deadline. Check explosion before processing a queued pass: at/after deadline,
explosion wins. Late responses cannot alter the exploded holder. Duplicate
requests resend cached decisions; duplicate commits cannot transfer twice,
add bonuses, or replay effects. Clients apply increasing host revisions and
request state when they detect gaps.

Send events, coarse tension changes, and infrequent liveness/recovery messages,
not continuous frame state. ACK/retry critical events with fixed retry count,
expiry, jitter, and per-peer bounds. Prioritize passes/terminal events above
beacons or bulk snapshots while maintaining bounded market service. Recovery
includes holder, roster, phase, revision, and outcome, never the hidden deadline.

### Explosion, rewards, and failures

At deadline host marks terminal state before settlement. Holder sees **YOU GOT
RUGGED**, brief flash, expanding circles, small particles, shake, and all six
LEDs flashing red conservatively. Others see **SAM GOT RUGGED / YOU SURVIVED**,
then their shared result. Return promptly to navy; no large images or blocking
animation.

Initial reward: survivor 2,000 GAME_SOL plus 100 per successful outgoing committed
pass, capped at five passes (+500). Exploded holder gets 250 consolation with
no pass bonus. Departed/forfeited players get zero. Apply 5,000 GAME_SOL/player/
epoch Bomb cap plus shared cap. No entry stakes or currency taken from players.

Explicit departure, or disconnect past a bounded reconnect grace, forfeits that
participant and removes them as a target. If holder forfeits, retain ownership
until explosion; never automatically transfer to a survivor. If fewer than two
connected eligible participants remain, abort without rewards. Briefly absent
players can recover canonical state before forfeiture, without extending time.

Host departure/restart cancels an uncommitted live round: no host migration or
speculative offline payout. Committed settlement is recoverable by wallet sync/
rejoin, subject to checkpoint durability. Incomplete start, missing challenge,
wrong key, and pending pass have clear failed/retry/cancelled states. Leaving
cancels local interaction, not a reward already committed by host.

**Optional only after reliable one-explosion play:** elimination removes the
exploded player and repeats with a new timer until one remains. Then consider
a capped +1 SOL winner bonus and bounded survival streaks. This extends RUG BOMB;
it is not a fourth game or a substitute for reliable basic multiplayer.

## 7. Shared rewards, validation, and persistence

Use one native `GameTicket / GameOutcome / RewardReceipt` path. Tickets bind
game type, rules version, player, market, unique round/puzzle ID, seed, issue
epoch, expiry, and reward policy. Results contain outcome, score/guesses, actual
credited GAME_SOL, authoritative wallet/revision, practice/cap reason, and best
flags. UI never assigns balances from client-provided rewards or animations.

Initial shared cap: **20,000 GAME_SOL/player per 120-second authority epoch**
across all games, plus game caps and `WalletCap`. These are playtest defaults,
not inherited Reaction limits. Charge budgets to ticket issuance epoch; expiry
and one-outstanding-ticket rules prevent banking old rounds for later epochs
or resetting allowances through rejoin. Persist current counters and a bounded
outstanding-epoch reservation. At the cap, offer labelled zero-reward practice.

MEMEWORD validates at most 30 submitted letters natively. RUG RUN replays its
deterministic rules against a bounded input trace, checking legal actions,
duration, collisions, score, seed, and rules version. Initial trace bound:
512 three-byte timestamp/action records, chunked only after the run through a
bounded assembler. Record state-changing input, not repeat noise. On exhaustion,
continue as practice and explain unavailable validated reward. Time-slice replay
so host input/deadlines stay responsive. Do not stream frame inputs or overwrite
snapshot scratch while a transfer is live. Bomb rewards use host-committed events.

Modified firmware can fabricate plausible single-player traces; this is casual
farming resistance. Replays, stale/altered proofs, reused sequences, aborted
rounds, and duplicate results cannot mint a second reward. Retries reuse identical
IDs/content. Guests show **Reward pending** until host acknowledgement. Host
outage permits practice, not local credit. A compact pending receipt can outlive
the game scene in Session, so leaving does not discard or duplicate a claim.

Persist wallet, caps, settled-ID high-water marks, consumed ticket identity,
replay/receipt state, puzzle schedule, best guess count, and Run best consistently
in one authority snapshot. Bomb settlement applies all roster rewards and the
terminal round ID as one main-task mutation/snapshot revision. No partial
per-player award after retry or reboot. Use bounded latest-result records,
not unbounded histories. Live animation, input traces, and unfinished Bomb
rounds are transient.

Bump wire and snapshot versions. Migrate valid v0.2.4 wallets, coins, holdings,
and identities; initialize new statistics/counters deliberately, invalidate
legacy Reaction tickets, and retain prior current-epoch reward spend against
the new shared allowance. Reject unsupported newer saves visibly rather than
wiping them. Preserve A/B CRC validation, inactive-slot write/readback, and
older-valid-slot recovery. Failed save keeps dirty state and blocks save-and-close.

Calculate maximum serialized size for 12 players/16 coins first. If 2 KiB is
insufficient, budget a bounded increase across codec, Session RX/TX, storage
scratch/verify buffers, transfer masks, and tests together. No implicit buffer
expansion or partition change. Power loss can roll back to the last complete
checkpoint; unsaved rewards are not crash-durable. Restart invalidates unfinished
tickets without resetting saved caps or settled-ID markers.

## 8. Visual language and market polish

### Palette and currency

Use compile-time RGB565 constants throughout, never convert RGB per draw.
These values use current `board::rgb` truncation; verify physical panel channel
order separately.

| Token | RGB888 | RGB565 |
| --- | --- | --- |
| Background | `#0B1020` | `0x0884` |
| Surface | `#151B2E` | `0x10C5` |
| SurfaceLight | `#202940` | `0x2148` |
| Text | `#F4F7FF` | `0xF7BF` |
| TextSecondary | `#8C96AD` | `0x8CB5` |
| Cyan | `#5EEBFF` | `0x5F5F` |
| Green | `#42E695` | `0x4732` |
| Red | `#FF5C6C` | `0xFAED` |
| Amber | `#FFD166` | `0xFE8C` |
| Purple | `#A78BFA` | `0xA45F` |
| Gold | `#FFC857` | `0xFE4A` |
| Blue | `#4DA3FF` | `0x4D1F` |
| Disabled | `#3B4256` | `0x3A0A` |

Green means success/profit/correct position/buy/victory. Red means sell/downward
movement/loss/damage/rug/error, distinguished by text and icons. Gold means SOL,
reward, valuable pickups; amber means partial word match. Cyan means focus,
interaction, connection; purple means special/meme power/coin level; blue means
information/neutral multiplayer; gray means inactive/unknown/absent. Never use
green as arbitrary selection color or scatter unrelated neon accents. Pair
state colors with text, shape, arrows, or symbols.

SOL icon/text is always **gold**, including balances and spent amounts. Earned
`+4.20 SOL` rises slightly and fades using cheap primitive/dither steps; spent
`-7.40 SOL` stays gold with subtle red/downward accent. Cache formatted integer
values in fixed buffers. Wallet precision is three decimals; use two for rewards
when exact. Never round a nonzero unit to zero or imply a different charged amount.

### Shared results

All games use one component: outcome, game name, score/guesses, best/answer as
applicable, **REWARD**, gold credited SOL, and **NEW BALANCE**. Show **A PLAY AGAIN /
B GAMES**, cyan for focused action, green for success, and explicit NICE TRY /
RUN OVER / YOU GOT RUGGED losses. Pending, capped, practice, and cancelled
results cannot masquerade as credited victories. Replay reuses the arena; B
frees game resources. Bomb Play Again returns to readiness/lobby, never starts
an individual unsynchronized round.

### Buy, sell, prices, and portfolio

Coin detail shows symbol, price, directional change with ▲/▼, and **YOU OWN**.
Default focus is Buy with **A BUY / B SELL**: A opens Buy quantity and B opens
Sell quantity. Up/Down can select Buy, Sell, Creator controls, or Back; A then
activates that selected action and its hint updates accordingly. B remains the
Sell shortcut only on coin detail. START returns to dashboard and HOME keeps
leave behavior. Do not interpret one B press as both Sell and Back.

Quantity panels show **BUY $GOOSE / Amount / Cost** or **SELL $GOOSE / Amount /
Receive**, with **A CONFIRM / B CANCEL**. Up/Down changes quantity; clamp bounds
and explain insufficient funds/tokens. Quotes include fees and actual precision.
A moving host price may invalidate the quote: use bounded spend/proceeds limits
or revision-bound quotes and refresh for reconfirmation instead of silently
accepting different terms. One request remains pending at a time.

On committed purchase, stay on the coin/confirmation view: **+5 GOOSE**, brief
green quantity outline, updated price/up arrow when actually higher, tiny screen
bump, and green/cyan LED pulse. Then **PURCHASED / 5 GOOSE / for 17.10 SOL**, using
actual receipt values. Feedback takes about 400–800 ms and stays interruptible;
no blocking three-second animation.

Committed sale shows **-5 GOOSE**, gold wallet increase, price down tick when
actually lower, short red accent, and single red outward LED pulse. Say **SOLD**,
not WARNING. Failed/rejected trades get explicit errors and no success effects.
Duplicate/reordered confirmations cannot repeat feedback: bind it to receipt IDs.

Cache price strings and dirty only changed digits/regions. Higher gets green ▲,
lower red ▼, unchanged neutral white. Do not redraw the full coin screen just
for a price update. Portfolio cards show white symbol, secondary-white quantity,
gold current value, and signed gain/loss with green ▲ or red ▼. Percent gains
require saved cost basis: add bounded per-holding cost accounting and proportional
basis removal on sells. Migrated positions lacking history say **Basis unavailable**;
never invent gain percentages from current value.

### Rug presentation

A committed coin rug briefly breaks the palette: red background flash, shaking
native coin icon, **RUGGED / $GOOSE / JON RAN OFF WITH / 47.32 SOL / YOUR BAG:
18 GOOSE / HOLD THIS L**. Show actual creator identity/alias and withdrawn fee
treasury in gold, without implying protected reserves were stolen. Return to
navy, keep a permanent red **RUGGED** badge, and preserve funded sells. Keep the
separate warning and fresh three-second A hold for creator rugging. Gameplay
prompts outrank remote rug decoration; retain at most one compact deferred alert.

### Animation, transitions, LEDs, and tactile feedback

Use small elapsed-time structs for position, rectangle scale, fill/outline,
and cheap optional dithering. No animation blocks input or requires a speaker.
Synchronize button timing, tiny bumps, tile snaps, visual shake, icon expansion,
color pops, and LEDs to suggest tactile feedback without audio.

Transitions last 150–250 ms: old panel exits, background clears, new panel enters.
Do not keep two complete scenes live to animate them. Target restart to playable
content within about 500 ms, excluding explicit countdown/network readiness.
Skip decorative transitions to meet input budgets when necessary.

| Event | Six-LED language |
| --- | --- |
| SOL reward | Short gold chase |
| Success | Green pulse |
| MEMEWORD solved | Green→gold celebration |
| Purchase | Green/cyan pulse |
| Sell | Single red outward pulse |
| Damage | One red flash |
| Shield | Cyan while active, dim/bounded |
| Rug / Bomb explosion | Brief rapid red flash |
| New high score | Gold→cyan→green |
| Multiplayer connected | Blue/cyan |

Keep brightness conservative, default dim, honor Profile/Aux off immediately,
and never leave six LEDs at full white. Effects expire/cancel on exit; RMT
busy/error stays nonfatal. Priority: gameplay/terminal cues, committed reward/
trade, navigation decoration. Screen text conveys everything with LEDs disabled.
Preserve brownout protection and verify battery-only play physically.

## 9. Implementation order and acceptance

The implementation follows these phases; acceptance gates remain the quality
contract, with measured results recorded separately. Game priority is **MEMEWORD → RUG RUN → RUG BOMB**,
after shared foundations. Under time pressure, cut optional themes, ducking,
elimination, streaks, and decoration before required quality. Exactly these
three games remain the final release target; label a partial build incomplete
instead of calling a placeholder game finished.

| Phase | Deliverable | Gate |
| --- | --- | --- |
| 0 — Foundation | Profile current firmware; palette, input repeat, arena, cards, result/receipt contracts, bounds/schema | Measured baseline; one active game; navigation/saved market behavior preserved |
| 1 — MEMEWORD | Flash dictionary, six-guess rules, directional entry, reveals, puzzle schedule, rewards/practice | Duplicate-letter/input tests; no replay payout; polished badge playthrough |
| 2 — RUG RUN | Fixed-step lanes/jump, fair chunks, pools, hearts, combos/powerups, validated scores/rewards | Reachable chunk boundaries; deterministic traces; stable hardware frame time and fast restart |
| 3 — RUG BOMB | Lobby, one-explosion mode, challenge/commit/recovery, atomic reward | Physical 2-, 3-, and 6-player rounds plus loss/reorder/duplicate tests; one canonical holder/result |
| 4 — Market polish | Buy/Sell feedback, dirty prices, portfolio basis, gold currency, rug alert, unified LEDs | Receipt-driven feedback; no accidental trades or color-only meaning |
| 5 — Qualification | Churn/soak/playtesting, migration, build/package, screenshots, player guide | Every quality gate passes; docs accurately describe the new build |

Reward/storage/protocol changes land with the first phase needing them, not in
a final integration rush. Update native CMake and test build inputs as modules
split. Preserve economy/network/leave regression coverage; replace legacy game
expectations with new contracts.

### Automated and visual checks

- **MEMEWORD:** row bounds, answer/guess coverage, blank/unknown guesses, repeated
  letters, wrapping, 400/800 ms acceleration, reveal input, solve/fail/replay
  rewards, pinned puzzles across epochs, restored IDs.
- **RUG RUN:** seeded replay, valid routes across thousands of seeds/difficulty
  levels, lane/jump collision timing, invulnerability, safe powerup expiry,
  pool exhaustion, score overflow, pause/resume, caps, trace truncation, and
  distinct loss/time-limit/quit outcomes.
- **RUG BOMB:** two to six simulated Sessions; full/unavailable lobbies, duplicate
  names, wrong sender/target/session/round, held/wrong/late challenge input,
  two-player return lock, commit/explosion race, loss/reorder/duplicates,
  bounded retries, disconnect/host loss, stale rejoin, result recovery, and
  exactly one terminal holder and settlement per round.
- **Economy/storage:** integer precision/caps, issue-epoch boundaries, idempotent
  proofs/receipts, no abort payout, atomic multiplayer settlement, migration,
  unsupported versions, corrupt/truncated saves, failed writes, A/B recovery,
  maximum snapshot, cost-basis rounding, and wallet sync.
- **Controller/rendering:** Games order, contextual buttons, pause/leave cancel,
  pending trade/reward exits, fresh-hold rug guard, clipping/readability,
  non-color cues, dirty regions, effect priority, dim/off, and released callbacks.

Once implemented, run `bash tools/test_native.sh` with sanitizers/warnings and
`bash tools/firmware.sh build` / `size`. Extend the native controller/render
harness for representative 320×240 screenshots and deterministic inputs. The
existing Lua browser simulator does not exercise these native games and cannot
serve as their verification.

### Physical completion bar

For every game, perform at least 100 enter/play/restart/exit cycles and a
populated-market soak. Record baseline/peak/post-exit heap and largest block,
arena/stack high-water usage, flash size, p50/p95/max frame/input latency,
RX drops, retries, and save latency. Post-exit allocations/state must return
to a stable baseline without a downward heap trend. Instrument no gameplay
file reads/flash writes, no scene-entry radio restart, no unbounded allocation,
and no game traffic after leaving except bounded pending settlement recovery.

Observe real LCD/LED behavior, battery power, LED-off play, populated-host play,
and 2–6 player radio contention. Verify host market service during games and
safe-point saves without lost pending state. Desktop data does not substitute
for physical LCD, RF, battery, or timing results.

A game is complete only when first-time players discover controls, understand
success/failure and SOL without explanation, spend the earned SOL on a coin,
and voluntarily choose another round. Playtest puzzle skill, arcade score
chasing, and social passing chaos separately. Tune rewards, pacing, challenge
windows, readability, and animation from these sessions. Stable frame time,
no leaks, fast restart, full resource cleanup, and necessary-only networking
are mandatory completion gates.

At release update [MARKET-README.md](../MARKET-README.md),
[firmware/README.md](../firmware/README.md), verification records, and generated
native distribution docs together; remove old game/control promises for the
new version. Keep retained Lua documentation clearly historical. The v0.3.0 player guide describes the implemented controls and rewards;
physical group play and battery endurance must be reported separately from
simulator/controller coverage.
