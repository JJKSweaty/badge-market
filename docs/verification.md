# Verification record

## Passing automated checks

- Lua syntax across every shipped module.
- **2,359 assertions** for curve arithmetic, symmetry, fees, replay protection,
  wallet/holdings accounting, withdrawals, rugs, eligibility, reward caps,
  snapshot integrity, serialization and malformed input.
- **3,000 randomized trades**, with snapshot decoding repeatedly checking
  reserve and aggregate-supply invariants.
- Fifty-plus launches across separate bounded test markets. A single market
  still caps at 16 lifetime coin slots; no test claims unlimited capacity.
- Two simulated badges: discovery, join, coin creation, lost/duplicate request
  retry, exact-once purchase, duel handshake/result, three-second rug hold,
  remote rug alert, failed save, and **300 scene cycles** with **12 widgets**.
- Delayed wallet reads cannot ACK a mutation. Host outage leaves the wallet
  unchanged; explicit retry uses the same transaction. Corrupt latest save
  falls back to an older valid slot.
- **11,000 malformed frames**, including valid-CRC random payloads, produce no
  unhandled error in the mock protocol path.
- Native allocator harness: **1,000 navigation cycles**, full player/coin
  capacity, then closing Lua returns tracked allocation to zero.

## Lua memory experiments

The harness opens base/string/table/math/utf8, a lightweight native SDK mock,
then the actual Lua modules. Mock widgets are userdata, not large Lua tables.
It measures allocator bytes, including compilation. SDK native heap, LVGL,
BLE, task stacks and DMA are **not** modeled. Real firmware library registration
can differ. These values are not actual badge heap measurements.

| Checkpoint | 32-bit WASI ABI, default 64-bit Lua numbers | 64-bit macOS ABI |
|---|---:|---:|
| Mock SDK baseline | 12,379 B | 17,186 B |
| Launcher, collected | 65,583 B | 80,550 B |
| Host + duel code, collected | 83,866 B | 104,297 B |
| Full 12-player / 16-coin market | 85,860 B | 106,479 B |
| All game code loaded, collected | 92,381 B | 114,168 B |
| After 1,000 cycles, collected | 92,666 B | 114,318 B |
| Peak during stress | 98,303 B | 132,210 B |
| Enforced test quota | 98,304 B | 147,456 B |

**Headroom is tight.** The 32-bit harness passes the manifest ceiling, but
its allocator approaches that ceiling before collection. It does not prove
the full native firmware has enough contiguous heap, nor reproduce a full
radio peer cache alongside every worst-case event. Actual system reserve and
native timings remain acceptance gates. Optional game code loading checks
Lua headroom and system free heap; busy hosts may decline it and recommend
playing on a client. Host startup also checks room before loading authority.

The 64-bit test quota is higher because its pointers/structures are larger;
this does **not** raise the device's 96 KiB manifest quota. `make test32` uses a
32-bit WASI build with the original 96 KiB ceiling. Neither test changes firmware.

Optimizations validated during this work:

- Packed authority records: player **68 bytes**, coin **27 bytes** before Lua
  string overhead. Full serialized snapshot **1,266 bytes**.
- One compact text blob with byte offsets; only visible copy is expanded.
- Boot-only UI/restore functions and the unused network-role handler are
  released after setup. Stock require's remaining module cache stays bounded.
- Market rendering reads packed holdings without allocating a table/player.
- Epoch processing allocates mutable player records only for actual holders.
- One reusable native widget pool; unchanged text/colors are not reapplied.
- No downloaded images, framebuffer, per-feature tasks, JSON market, or
  external backend. Packet sizes are 13–38 bytes, below the 44-byte API cap.

## Browser check

The local two-badge simulator loaded, showed both 320×240 views and controls,
and completed host coin creation → nearby discovery → join → market retrieval
through browser buttons. No browser errors were reported. Screenshots are in
`build/simulator-start.png` and `build/simulator-market.png` (development files).
The simulator uses the actual Lua with mocked SDK functions; it is not the
physical LCD renderer or a BLE performance test.

## Physical badge status

A connected USB badge initially returned the stock console help and LVGL
startup text. It exposed `heap`, `apps`, `put`, `reload`, `radio`, and `shot`.
Brave then owned its serial port. After the user disconnected it, subsequent
diagnostic commands produced **no response**, even with raw 115200 serial and
DTR. A normal power cycle was requested. No new app has been installed, no
firmware flashed, and no erase/reset command sent.

Still unmeasured: firmware version, free/minimum system heap, largest block,
live Lua peak, BLE allocation, native widget allocation, flash write latency,
input latency, callback timings, partition table, and actual two-badge radio.
Flash firmware delta is zero; the app package's exact source bytes are recorded
by `make package` in `dist/manifest.json`. Original partition offsets were not
assumed or modified.

## Physical acceptance sequence

1. Read `heap`/`radio` before launch; record firmware from startup log.
2. Install the new slug using the console helper; verify `reload`, then launch
   and inspect `uitree`/screen and preceding errors (launch log alone is not proof).
3. Capture startup, host, radio, Reaction Trader, market and save statistics.
4. Verify buttons, LED positions/brightness, tilt direction and fallback D-pad.
5. On two badges verify discovery, request loss/retry, duel, holder epoch and rug.
6. Save, Home, reopen; test both slot recovery paths and unavailable sensors.
7. Repeat navigation and games, record minimum free heap/largest block and max
   callback time. Adjust limits to retain a meaningful real system reserve.

Until that sequence is run, describe this as a tested implementation with a
physical-device gate outstanding—not production-certified firmware.
