# Badge Market — native ESP32-C3 firmware

For a step-by-step two-badge walkthrough, read the
[Market player guide](MARKET-README.md).

Standalone C++17 game for the 2026 Hacker Badge, built with **ESP-IDF 5.5.3**.
It implements the hardware drivers and ESP-NOW radio itself. No Lua interpreter,
stock badge API, external components, Wi-Fi access point, phone, or internet is
needed while playing. Native badges communicate with other native badges on
**channel 6**; the earlier stock Lua/BLE app is not protocol-compatible.

## Open and build

Open this `firmware/` directory in VS Code. Use **Terminal → Run Build Task**
(Cmd+Shift+B) to run **Badge: build** with the pinned local toolchain. The tasks
work independently of the extension's selected ESP-IDF installation. Do not
create a new project or replace this project's CMakeLists.txt.

From the repository root:

```sh
bash tools/setup_esp_idf.sh    # once on a new computer; already installed here
bash tools/test_native.sh
bash tools/firmware.sh build
bash tools/firmware.sh size
python3 tools/package_firmware.py
```

The application is `firmware/build/badge_market.bin`. Flashing also requires the
matching bootloader and partition table; the ZIP in `dist/` includes all three
and a SHA-256/offset manifest. It is not an online Lua IDE import file.

The packaged segment offsets are `0x0` for `bootloader.bin`, `0x8000` for
`partition-table.bin`, and `0x10000` for `badge_market.bin` (DIO, 80 MHz, 4 MB).
Do not flash the application-only file at offset zero.

## Flash the badge

This replaces the stock firmware. Its launcher, contacts UI, Lua apps, BLE
services, NFC features, and other stock features will no longer run. On first
native boot the game may initialize its save partition at `0x2b0000`, which
overlaps the old stock filesystem. Keep the full backup to restore everything.

1. Close the online IDE and other serial connections. Use a USB data cable.
2. Run **Terminal → Run Task → Badge: backup and flash**, or from the repo root:

   ```sh
   bash tools/firmware.sh flash /dev/cu.usbmodem1101
   ```

3. The script builds, reads the complete 4 MiB flash into `build/backups/`, checks
   its size, records its SHA-256, and only then flashes. Backups can contain
   private badge data and are excluded from Git. A failed backup stops flashing.
4. If the ROM connection times out, power off, hold **Start** while connecting
   USB/powering on, release it, and retry. A blank screen is normal in download
   mode. The serial device name can change; check `ls /dev/cu.usbmodem*`.
5. After flashing, power-cycle normally **without Start** if it stays blank.
   The game boots directly into its Solo / Host / Join menu.

Monitor with `bash tools/firmware.sh monitor PORT`; Ctrl+] exits. Console commands:
`help`, `status`, `save`, `press A`, `press B`, `press HOME`, `press UP`,
`press DOWN`, `press LEFT`, `press RIGHT`, `press AUX`, `press START`,
`down START`, `up START`. `status` reports runtime free/minimum heap, RX drops,
radio state, transmit power in quarter-dBm, and the ESP-IDF reset reason.

To restore a full backup, activate the same toolchain in a Bash terminal:

```sh
export IDF_TOOLS_PATH="$PWD/.tools/espressif"
source .tools/esp-idf/export.sh
python -m esptool --chip esp32c3 --port /dev/cu.usbmodem1101 write_flash 0 build/backups/YOUR_BACKUP_FOLDER/flash.bin
```

The restored image must be this badge's complete 4 MiB backup. Use the same
download-mode procedure if needed. No `erase_flash` is necessary.

## Play

Boot: Up/Down and A choose Solo, Host, or Join. Host stays active while clients
trade. Join discovers nearby native hosts; match the code displayed on the
host's dashboard. Home, or B at the dashboard, opens a leave prompt. Confirm to
save (Host/Solo), disconnect, and choose another mode without restarting. Solo
and Host have separate checkpoints.

| Screen | Controls |
| --- | --- |
| All menus | Up/Down moves highlight, A selects, B returns to previous screen |
| Hub | Choose Market, Minigames, Portfolio, Launch a coin, Duels, or Profile |
| Market | Up/Down select; A opens coin |
| Coin | Highlight Buy, Sell, Quantity, Creator controls, or Back; A selects |
| Quantity | Up adds one, Down subtracts one, A done |
| Create | Select a letter with A, Up/Down edits, A done, B cancels edit; select Launch when ready |
| Creator | 10/25/50% fee withdrawal; rug requires holding A for 3 seconds |
| Reaction | Wait for cue, then A BUY / B SELL / Up HOLD; 8 rounds |
| Tilt Vault | Tilt or D-pad moves @ to $; practice, no SOL reward |
| Duel | Select trader; A challenge/accept/react, B cancel; no SOL stakes |
| Profile | Select Save, LEDs, Brightness, or Back with Up/Down and A |
| Global | Home opens leave prompt, Start hub/back, Aux toggles LEDs |
| Leave prompt | Up/Down selects Keep playing or Leave; A confirms, B cancels |

The rug warning requires releasing the entry A press, then holding A for three
seconds. Releasing cancels the countdown. B cancels; Start returns to the hub.
LEDs add feedback without changing the controls:

| LED effect | Meaning |
| --- | --- |
| Gentle green chase | Idle / active market |
| Brief cyan tick | Menu action |
| Green / blue | Confirmed buy / sell |
| Purple chase | Coin launched |
| Gold chase | Reward or withdrawal confirmed, maze complete, or duel win/tie |
| Red | Rejected action or missed reaction |
| Slow red pulse | Rug alert or rug confirmation screen |
| Blue chase | Finding a host or waiting for a trade response |
| Reaction: green / red / blue | BUY / SELL / HOLD, matching the screen |
| Duel: dim blue, then gold | Wait, then HONK / react |
| Teal | Save confirmation; maze lights track movement |

Reaction and duel cues override decorative animations. Repeated packets do not
replay trade confirmations; only committed results trigger them. Effects expire,
use no heap allocation or blocking delays, and stay at modest brightness. Profile
settings and Aux disable all LED output; Dim reduces every effect and is the
default on startup. Nothing
requires seeing the LEDs. The accelerometer can fail without stopping the game;
the D-pad remains available.

## Battery power

Version 0.2.3 keeps Wi-Fi off at the first menu and in Solo. Host and Join start
ESP-NOW on demand; backing out of discovery stops it. Startup PHY power is capped
at 10 dBm (the SDK's minimum configuration value), then normal transmit power is
capped at 5 dBm. Reception remains continuous for reliable nearby discovery.
This reduces radio transmit peaks; it does not establish a measured battery
lifetime. Range may be shorter than the previous maximum-power build.

The reported battery reboot displayed `LAST RESET: POWER DROPPED`, confirming a
brownout. Brownout protection remains enabled. If that message recurs with this
build, try fresh matched AA cells and check the battery contacts. A supply that
still sags needs a battery/power-path check; firmware cannot maintain voltage.
The console reports `radio=0 tx_qdbm=-1` while off and `radio=1 tx_qdbm=20` while
active. A radio setup error stays on the menu with a retry message.

The economy preserves the Lua version's rules: 25 starting fake SOL, 2 SOL coin
creation, linear bonding curve, 2% trade fees, creator/community fee pools,
delayed holder rewards every 120 seconds, one active coin per creator, and rug
reputation penalties. Reserves cannot be withdrawn; exits remain funded after
a rug. Limits: 12 players, 16 lifetime coins, 4 holdings/player, 10,000 supply.
Reaction rewards cap at 2 SOL/game and 6 SOL/epoch, with a 20-second cooldown.

## Architecture and verification limits

- `core/`: portable economy, validated binary snapshots, transaction sequencing,
  ESP-NOW application protocol, reaction proofs, and duels. Fixed-size records;
  no game-state heap allocation. The host alone mutates the canonical market.
- `main/board.cpp`: ST7789 via SPI/DMA, 10 KiB stripe buffer, 5x7 font,
  HC165/Start debounce, SC7A20 I2C with timeouts, WS2812 RMT. NFC is unused.
- `main/radio.cpp`: station-mode ESP-NOW, one broadcast peer, 250-byte maximum,
  bounded 16-packet receive queue. Callbacks only copy data. Sends wait for the
  previous completion callback; there is no network work inside an ISR.
- Complete snapshots travel in 200-byte chunks with revision, offset, length and
  CRC validation. Partial snapshots never become visible. Requests retry the
  same sequence and payload; snapshots acknowledge their committed result.
  Dropped ACKs cannot duplicate trades. A missing host leaves a trade pending.
- Save storage: dedicated 64 KiB NVS partition, two validated snapshots per mode;
  write the older slot, commit, then read back. Checkpoint every 30 seconds when
  dirty, on confirmed leave, and Profile Save now. Power cuts may roll back
  since the last save. A save failure blocks leaving the host/solo session.
- Broadcast radio is unencrypted; MACs and reaction reports are not authenticated.
  This is a friendly game, not an adversarial financial system. There is no host
  migration, offline trading, stock-BLE interoperability, or automatic rollback
  recovery for already-connected clients. Restart clients after restoring a host.

Host tests exercise 30,000 random trades with conservation checks, corrupted
snapshots, duplicate requests, reaction tickets, and two peers under packet loss,
duplication, a host outage, and duel handshakes. AddressSanitizer and
UndefinedBehaviorSanitizer are enabled. A successful ESP-IDF build checks the
actual ESP32-C3 ABI and drivers; it is not evidence of physical LCD orientation,
button wiring, radio range, power draw, or runtime heap. Those require badge
testing. See
[`VERIFICATION.md`](VERIFICATION.md) for the exact hardware checks and remaining
limits, including battery validation of the lower-power settings.

Hardware reference: [official HAL guide](https://badge.hackthenorth.com/custom-firmware-hal.md).
Radio reference: [ESP-IDF 5.5.3 ESP-NOW](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32c3/api-reference/network/esp_now.html).
