# Stock-firmware implementation

The supplied app guide takes precedence over assumptions in the original
game brief. Firmware already provides native LVGL transport/rendering,
debounced input, cached accelerometer, LED driver, file storage, execution
deadlines, allocator quotas, and streamed app sharing. Lua cannot extend native
bindings, use ESP-NOW, modify DMA buffers or inspect partitions. Its radio is
BLE, with **44-byte** binary-safe broadcasts and an 8-frame receive ring.

Use existing firmware; do not flash or rewrite drivers. The main source is
split into a few sandbox `require` modules because the original brief explicitly
requires modular code. The stock module cache cannot be unloaded from Lua:
code is loaded on demand and retained until app exit; only one scene's *state*
is live. Reuse one small widget pool across all scenes. The bundle must fit the
48 KiB / 16-file Share cap and leave room for private A/B saves within 64 KiB.

Limits: 12 players, 16 coin slots, 4 holdings/player, 8 nearby peers, 8 cached
coins/client, one RPC in flight, 44-byte frames. Full canonical slots are never
evicted. Capacity returns a visible error. A later native firmware build can
raise these limits. The host must keep the app open. Markets cannot merge.

All currency is fake GAME_SOL. No wallet, external service or internet API.
Start with 25 SOL, launch fee 2 SOL, base 0.100 SOL, slope 0.005 SOL/token,
supply cap 10,000, 2% trade fee split between creator/community. Separate curve
reserve from fees. Only the fee treasuries can be rugged; sells remain funded.
Epochs are 120 seconds of host foreground uptime, persisted without offline
catch-up. Reward eligibility and integer rounding are tested on desktop.

The host validates requests, enforces sequential transaction IDs, caches the
last response, and binds accounts to observed radio MACs. One retry resends
identical bytes. Replies echo sequence and market identity. Sequence overflow
requires a new market; never silently wrap. CRC detects corruption, not malicious
forgery. A modified badge can spoof MACs or plausible game input: friendly-game
trust only, not a cryptographic security boundary.

Persist explicit versioned binary data with CRC into appdata A/B files.
Validate the inactive file before choosing its higher generation. Periodic
checkpoints and normal HOME exit bound flash wear. A sudden power loss can roll
back unsaved trades. Save failure stays visible. Player identity comes from the
radio MAC when enabled; local solo identity is separate until joining/hosting.

Hardware baseline: console confirmed an LVGL launcher and installed app APIs.
At the next diagnostic read Brave owned the USB port. Heap, largest block,
radio allocation, Lua peak and physical latency are **not measured yet**.
Desktop allocator tests establish bounds, not real badge heap availability.

The stock app version uses a 96 KiB *ceiling*, not reserved RAM. It logs live
usage, peak, widget count, system free heap, tick max and packet drops. Physical
acceptance requires startup, radio start, host load, buy/sell, save, HOME,
reopen, and hundreds of scene transitions without falling heap trends.

See `verification.md` for the passing 32-bit quota experiment and its tight
headroom. Setup functions and the unused network-role handler are released
after role selection. `game/copy.txt` holds stable text IDs; `build_copy.py`
generates one compact indexed text blob instead of hundreds of retained strings.
Before loading authority, require 28 KiB of Lua headroom and 64 KiB system free
heap. Optional game modules require 11 KiB Lua headroom and 48 KiB system free
heap. These are provisional admission guards, not measured device reservations.
