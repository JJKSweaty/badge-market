# Native firmware verification — 2026-09-20

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
