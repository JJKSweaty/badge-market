# Native firmware verification — 2026-09-20

## Battery follow-up and demo — v0.3.1

The user reports intermittent battery Host resets after the earlier power cap.
The prior abnormal reset text was POWER DROPPED; no voltage/current trace is
available. Inspection found that the native app initialized only `bm_store`.
The SDK's PHY calibration uses default NVS. The v0.3.0 log contains
`failed to load RF calibration data (0x110f), falling back to full calibration`.

Changes:

- Initialize default NVS before Wi-Fi, preserving its contents and returning a
  menu error on initialization failure. Do not erase it automatically. Retained
  calibration avoids unnecessary full-calibration fallback on subsequent boots.
- Reduce runtime TX limit from 5 to 2 dBm (8 quarter-dBm, the supported API
  minimum). Startup PHY cap stays at 10 dBm. Brownout protection stays enabled.
- Suppress unchanged unsolicited full snapshots at an unattended host, while
  continuing discovery beacons. Changed revisions and guest resync requests
  still send snapshots; wire format and game rules remain v0.3 compatible.

Sanitizer tests pass for idle-host beacons without repeated snapshots, simulated
lossy trading/recovery, all three games, the controller and renderer. Build
passes for ESP-IDF v5.5.3 with an 811,040-byte application. The complete pre-flash
backup is `build/backups/badge-nqEex53X/flash.bin` (4 MiB, SHA-256 verified).
Flashing verified the application/bootloader/partition hashes.

The three-minute recording plan is `DEMO-PLAN.md`; its ten consecutive cuts total
180 seconds. STARE and GOOSE are valid dictionary words. The example answer is
illustrative; the plan calls for recording actual puzzle outcomes/rewards.

References: local IDF `components/esp_phy/src/phy_init.c` calibration/NVS path;
[Espressif TX power API](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32c3/api-reference/network/esp_wifi.html#_CPPv425esp_wifi_set_max_tx_power6int8_t).
These fixes reduce avoidable radio work; battery stability still requires a
battery-only test. Use USB power for the demo until that test succeeds.

Connected-badge result (`build/native-v031-power-check.log`): both Host entries,
separated by a USB hardware reset, queried TX cap 8 and loaded the PHY without
the previous full-calibration fallback. The subsequent Host session ran for
more than 180 seconds on USB, including MEMEWORD entry/edit/pause/resume and
RUG RUN through a credited result. Free heap stabilized at 151,160 bytes,
minimum 150,796, RX drops zero, and main stack headroom 8,208 bytes. No reset
occurred. RUG RUN rendering measured median 23 ms, p95 33 ms, maximum 43 ms
across 324 steady-gameplay renders; this is not a guarantee of sustained FPS.
Save and exit passed, and the serial port was released. This was one badge on
USB, not a battery or physical multiplayer test.

## Native games — v0.3.0

The native firmware now contains exactly MEMEWORD, RUG RUN, and RUG BOMB.
Reaction Trader, Tilt Vault, and Badge Duel have been removed from its runtime.
The retained Lua app is a separate legacy target.

Implemented and checked:

- A 353-answer / 704-guess dictionary stored as 5,285 bytes of fixed-width flash
  data; duplicate-letter scoring, six attempts, accelerated directional entry,
  reveal/solve feedback, and one reward per puzzle ID.
- A fixed-step, 40-second, three-lane runner with jump, three hearts, invulnerability,
  combos, three timer/flag powerups, fixed pools, replay validation, score caps,
  and diminishing rewards. 2,000 seeded routes survived using the generated
  safe path; their recorded inputs reproduced the exact score.
- Host-authoritative 2–6-player Bomb lobby/readiness, private explosion seed,
  fresh-button challenges, committed transfers, pass-back lock, wrong/missed
  challenges, bounded recovery, and idempotent terminal rewards. Two-, three-,
  and six-node simulations pass with loss, reordering, and duplicate delivery.
- Shared navy/cyan cards and results, semantic RGB565 colors, gold currency,
  explicit Buy/Sell confirmations with quote limits, receipt feedback, portfolio
  cost basis, permanent rugged status, and conservative LED effects.
- One active game arena, measured at 1,824 bytes on the badge. No per-obstacle
  allocation. Two 5 KiB DMA buffers, command-damage fingerprints, and 8-row stripe
  hashing; no framebuffer. 400 randomized incremental renders exactly match
  full renders, including moved/removed/overlapping primitives.
- The existing 30,000 trade/conservation checks, 20,000 malformed/corrupt snapshot
  cases, malformed radio tests, lossy market transactions and host-outage recovery.
  The actual controller passes game navigation, pause/leave, failed-save resume,
  immediate replay without duplicate reward, trade confirmation, client exit,
  and 100 game entry/exit cycles. AddressSanitizer/UndefinedBehaviorSanitizer and
  compiler warning checks pass.

The new full 12-player/16-coin snapshot is **1,928 bytes**, within the existing
2 KiB transfer/save limit. v0.2.4 migration is tested against its exact binary
schema, retaining balances, coins, identities and holdings while invalidating
old tickets. Historical cost basis is explicitly unknown. The partition table
is unchanged. Restore a full old backup to downgrade; old firmware cannot read
new snapshots.

The ESP-IDF v5.5.3 build passes for ESP32-C3 without compiler warnings. The final
application image is **810,976 bytes**, leaving **71%** of its application
partition unused. The main task stack is 24 KiB to cover bounded snapshot and
proof-validation call stacks. Host ABI sizes are Market 3,912 bytes and Session
8,672 bytes; these are not runtime heap measurements.

The connected badge was backed up in full before every flash. The pre-update
v0.2.4 backup is `build/backups/badge-w2u3YaG9/flash.bin`; the final-install backup
is `build/backups/badge-xWluNmpB/flash.bin`. Each is 4 MiB with a recorded SHA-256.
The final v0.3.0 flash passed esptool readback hash verification. Boot identifies
v0.3.0, and the existing market/coin restored.

The first USB smoke sequence passed Host restore, MEMEWORD entry/edit/pause/Home,
RUG RUN through its credited result, Bomb lobby, repeated game entry/exit,
save/reopen, and radio-off exit. Heap stayed near 153 KiB with no RX drops.
An initial 57 ms mixed-screen p95 prompted the incremental/pipelined renderer
change. Subsequent mixed-screen average fell to about 22 ms, with roughly
152 KiB host free heap and over 8 KiB main-task stack headroom; that run's USB
connection was interrupted during later scene cycling. Mixed-screen percentiles
include full-screen transitions and are not a steady-gameplay FPS measurement.
The final build adds separate `RUN_METRICS` to measure that distinction.

The complete final USB sequence passed: Host restore, MEMEWORD controls,
pause/resume and leave cancellation, RUG RUN with a credited result, Bomb lobby,
four further Word entry/exit cycles, save/reopen, and exit to the first menu with
the radio off. The serial connection was released normally. Host free heap was
151,736–151,868 bytes, minimum 151,348 bytes, main-task stack headroom 8,132 bytes,
and RX drops stayed at zero. The saved wallet restored at 27.547 game SOL.
Across 316 steady RUG RUN renders, median was **23 ms**, p95 **35 ms**, and maximum
**41 ms**. This narrowly misses the 33 ms render target at p95; a sustained 30 FPS
claim is not established. Full-screen transitions reached 61 ms. These counters
measure rendering duration, not input-to-photon latency.

Logs: `build/native-v03-usb-check.log`,
`build/native-v03-optimized-usb-check.log`, and
`build/native-v03-release-usb-check.log`; the final flash is recorded in
`build/native-v03-release-flash.log`. Screens were rendered from the actual
native painter at 320×240 and inspected. This does not verify the physical LCD's
color accuracy or LED appearance.

Remaining physical gates: actual 2–6-badge RF interaction/range and contention,
battery-only endurance, optical/first-time-player playtesting, and a longer
populated-market soak. Simulated multiplayer and USB tests must not be described
as physical multi-badge validation. Optional themes, user-entered aliases,
ducking, and elimination mode remain outside this release.

## Earlier native verification history


Built using the real ESP-IDF **v5.5.3** RISC-V toolchain, target **esp32c3**, with
the committed `sdkconfig.defaults`. Final build completed without compiler
warnings. Application image is approximately **754 KiB**, leaving **72%** of
the 0x2a0000 application partition unused.

The linker size report shows **129,840 bytes** of DRAM occupied by linked code,
data and BSS, with **191,456 bytes** remaining in that linker region. This is
**not runtime free heap**: task stacks, Wi-Fi buffers, queues, and the LCD DMA
stripe consume memory after startup. Use `status` over USB and measure minimum
free heap during a populated multiplayer market on the physical badge.

Game storage and memory are bounded: the host ABI test reports a 1,520-byte
Market, a 6,080-byte Session (including two 2 KiB transfer buffers), and a
**1,272-byte serialized full market** containing 12 players and 16 coins. C++
ABI padding can differ; wire serialization does not use native structure layout.
The LCD uses one **10,240-byte** internal DMA stripe, never a full-frame buffer.

`bash tools/test_native.sh` passes with AddressSanitizer, UndefinedBehaviorSanitizer,
and compiler warnings treated as errors. Tests include:

- 30,000 random trades with supply/reserve invariants and total-asset conservation.
- Idempotent duplicate requests and rejection of changed payloads reusing a sequence.
- Rug ownership checks, closed buys, funded fee-free exits, and holder rewards.
- 12-player/16-coin capacity, save round trips, checksum corruption, and truncation.
- 10,000 random save inputs and 10,000 mutated snapshots with recomputed CRCs.
- Reaction ticket cooldown, seed-derived proof, reward, replay, and early-press handling.
- Two Session instances with loss, reordering and duplication: discovery, join,
  coin creation, purchase, host outage with no speculative wallet mutation, recovery.
- Duel invitation/accept/start/result retries and rejection of late duplicate invites.
- 10,000 malformed radio frames and cancellation of an incomplete join.

## UI and connected badge follow-up

Version 0.2.1 uses Up/Down/A/B menu navigation throughout the hub, coin actions,
creator controls, profile, and coin-name editor. A separate quantity screen uses
Up/Down to adjust and A to finish. The actual firmware controller is compiled
against host hardware stubs in `tests/menu_test.cpp`, with sanitizer checks for
Host startup, navigation, coin creation, buying/selling, settings, and the
release-before-hold rug confirmation. Minigame action buttons remain explicit
on their screens.

The connected native badge was backed up in full before the update. Its game
save partition was preserved. The earlier 0.2.0 Host session remained active on
USB through autosaves, with about 180 KiB runtime free heap; the reported return
to the initial menu was not reproduced, and its cause remains unconfirmed.
Version 0.2.1 displays abnormal reset reasons and logs startup stages. Optional
LED transfers now skip busy/error frames rather than aborting the game; the
in-flight RMT buffer is retained until the driver reports completion. This fixes
an avoidable fatal-error path but is not proof of the reported reboot's cause.

Remaining checks include physical two-badge range/loss, battery-only behavior,
visual layout on the LCD, and capturing a reset log if the reported issue recurs.
The native radio cannot talk to the previous Lua/BLE version.

Connected-badge result: the final 0.2.1 build remained in Host for **80 seconds**,
including two autosave intervals and an explicit save. Free heap stayed at
**179,832 bytes**, minimum **179,508 bytes**, RX drops **0**, with no reset or
panic in the captured USB log (`build/native-host-soak.log`). This was one badge
on USB, not a battery or two-device radio endurance test.

## Gameplay LEDs — 0.2.2

Added one fixed-size transient effect controller and completion-operation metadata.
Effects use committed transaction sequences (including network acknowledgements),
not optimistic button presses. Tests cover duplicate completion suppression,
failed trades, reaction/duel cue priority, brightness bounds, expiry, rug priority,
dimming, and complete blackout when disabled. The RMT driver remains asynchronous
and optional LED errors do not abort the game. Firmware build and sanitizer tests
pass. Physical visual color/order still requires observation.

0.2.2 was installed after a fresh verified full-flash backup. The restored Host
market stayed responsive for a 32-second smoke test with menu feedback and LED
off/on commands. Free heap remained 179,800 bytes (minimum 179,476); no reset or
LED driver errors appeared in `build/native-led-smoke.log`. This verifies command
execution and stability, not optical appearance or two-badge operation.

## Battery brownout mitigation — 0.2.3

The user confirmed the battery reboot displayed `LAST RESET: POWER DROPPED`.
This identifies a brownout, although the supply voltage/current has not been
measured. Replaced the boot slogans with a plain description of the game.

- Startup PHY limit reduced from 20 to 10 dBm, verified in generated sdkconfig.h.
- Runtime Wi-Fi TX cap is 20 quarter-dBm (5 dBm), applied before ESP-NOW starts.
- Radio stays off on boot and in Solo; Host/Join enable it on demand. Cancelling
  discovery deinitializes ESP-NOW before stopping Wi-Fi, then clears queued data.
- Failed radio setup cleans up and returns a menu error instead of aborting.
- LED effects start dimmed. Brownout detection remains enabled.
- USB status now includes reset reason, radio state, and queried TX power.

ESP-IDF build passes without compiler warnings; application size is 769,808 bytes.
Sanitizer tests pass, including actual-controller tests for offline Solo,
Join/back radio lifecycle, and a failed Host radio start followed by retry.
These checks do not establish battery stability or RF range; physical battery
and two-badge testing are still required.

Installed on the same badge (MAC ending `e8:c8:0c`) after a verified 4 MiB backup
at `build/backups/badge-SVCCSxXi/flash.bin`. The physical USB check confirms:

- Cold boot and Solo: radio off, 221,056 bytes free heap.
- Six Join/back cycles: radio starts with queried TX cap 20 quarter-dBm and stops
  on every cancellation, with no reset.
- Restored Host: stayed responsive for more than 70 seconds, including save,
  at 179,812 bytes free heap, minimum 179,488, and zero RX drops.

Log: `build/native-battery-usb-check.log`. The USB console has been released for
the user's battery-only retest; no successful battery result has been claimed.

## Navigation, display and player guide — 0.2.4

B now backs out through the menu hierarchy and opens the leave prompt at the
dashboard. Home opens that prompt from any active market screen. Host/Solo must
save successfully before leaving; client exit clears local pending state without
writing a host checkpoint. Leaving shuts down ESP-NOW and resets Session,
minigames, discovery and feedback so a different mode can start without rebooting.
Cancelling the prompt restores the previous screen/selection and letter edit.

Host and client dashboards show the same four-character host code. Empty markets
offer Launch first coin; Portfolio fills the first free slot; coin Back remembers
whether it was opened from Market or Portfolio. Reaction B remains its SELL cue
during play and returns to Minigames once done. Late game tickets do not reopen
the reaction screen after navigating away.

The renderer now uses warm off-white, dark ink, muted green, and an ochre selection
marker. It still uses one 10 KiB stripe, with no full-screen framebuffer. Its
portable drawing code is shared with the host preview runner. Boot, Host, trade
and leave screens were rendered at 320x240 and visually inspected; this does not
verify the physical panel's color accuracy.

The ESP-IDF build passes without compiler warnings (771,600-byte application).
Sanitizer tests pass, including actual-controller tests for Home/B paths, cancelled
exit, failed-save retry, host-to-solo switching, client leave during a pending
request, editor restoration, empty-market navigation and the shared renderer.
The existing simulated two-node loss/duplicate/trading tests also pass.

`MARKET-README.md` is the player guide, also included in the firmware archive.
Physical two-badge connection/trading and battery endurance remain unverified.

Installed after a verified 4 MiB backup at
`build/backups/badge-3o5kbE6Z/flash.bin`. USB-injected button tests on the badge
passed Coin → Market → dashboard Back, B-to-leave/cancel, Home-to-leave/cancel,
Host → first menu → Solo → first menu → Join, and three additional saved Host
reopen/close cycles. The saved coin remained present. No reset or RX drops;
Host heap stabilized at 179,780 bytes, minimum 179,448. The test ends at the
first menu with radio off and releases the serial port.
Log: `build/native-navigation-usb-check.log`.
