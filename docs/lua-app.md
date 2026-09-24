# Badge Market — legacy Lua app

A badge-native meme-coin game for the **stock 2026 Hacker Badge firmware**.
Fake SOL, nearby markets, reaction duels, diamond hands, and creator betrayal.
No phone, server, wallet, cryptocurrency or internet connection is required.

The implementation is in [`app/badge_market`](../app/badge_market). The installable
bundle is [`dist/badge_market.zip`](../dist/badge_market.zip). It contains all nine
required files. This is a modular app, not a single-file IDE import.

## Play

1. On the first badge choose **Host nearby market**. Keep the app open.
2. On another badge choose **Join nearby market**, select the host, press A.
3. At the hub: A opens Market, B opens Games, Up opens Portfolio, Down launches
   a coin, Left finds nearby traders, Right opens your profile and save status.
4. In a coin: A buys, B sells, Left/Right change quantity, Up returns to Market.
   Creators press Down for treasury controls.
5. Rugging requires a separate warning screen and **holding Start for 3 seconds**.
   Releasing Start cancels the countdown. Other badges get a rug alert.

**Start** returns to the hub, or retries an unresolved request using the same
transaction ID. **Home** saves and exits to the launcher. **Aux1** toggles LEDs;
Left on Profile dims them. Buttons use pressed events, except hold-to-rug.

Reaction Trader: wait for the command, then **A = BUY, B = SELL, Up = HOLD**.
Eight seeded rounds award up to 2 SOL, with a 20-second cooldown and a 6 SOL
epoch cap. Early presses lose the round. Badge Duel compares each player's
local reaction time after HONK; it awards bragging rights, not currency.
Tilt Vault is a small practice maze: tilt or D-pad to move `@` to `$`.

The lime LED chase means active play; green/red/blue indicate BUY/SELL/HOLD.
A slow red pulse marks the rug warning/alert. Effects stop on exit. LED
brightness settings are session-only. Nothing depends on seeing the LEDs.

## Install without replacing firmware

Save any existing IDE workspace first. Close serial connections in browser
tabs before using the helper. On macOS/Linux, from this repository:

```sh
python3 tools/badge_console.py --port /dev/cu.usbmodem1101 --upload app/badge_market
```

The helper uses the same `mkdir`, `put`, and `reload` console flow as the
[official Badge IDE](https://badge.hackthenorth.com/ide/). It installs only
`/littlefs/apps/badge_market/`; it does not erase or flash firmware. Select
Badge Market in the launcher and press A. Reopen after ordinary code updates;
reboot after changing runtime options on an already installed manifest.

Alternatively, in the Badge IDE replace `manifest.cfg` and `main.lua`, then use
the files panel's **+** to add `codec.lua`, `market.lua`, `session.lua`, `ui.lua`,
`games.lua`, `duel.lua`, and `copy.lua` with their complete contents. **Connect → Push**.
The current Import app button handles one config/code file, not this ZIP.
All modules must be present. No images or external packages are needed.

Once installed, use the badge's **Share → Send an app** to send the complete
bundle. On the other badge use **Share → Receive an app**, then accept once.
Private save files are not shared. Stock firmware may reboot when leaving a
radio app to release BLE memory; this preserves installed apps and saves.

## Persistence and limits

The host owns wallets, holdings, coins, reputation, reward admission, and the
transaction sequence. Solo and nearby markets have separate saves and do not
merge. A host must remain in the app; offline clients cannot trade. There is
no automatic host migration. The radio uses the stock BLE API, not ESP-NOW.
Frames identify participants by their observed radio MAC; names/contact data
are not broadcast. MAC identity and game timings are not cryptographically
authenticated: this is a friendly hackathon game.

Host capacity: 12 players, 16 lifetime coin slots, 4 holdings/player. Nearby
lists cap at 8 entries; client detail cache caps at 8. Full canonical slots
return errors instead of evicting other people's balances. These are deliberate
stock-runtime limits. Hot Potato, arbitrary coin-associated code, coin art,
and advanced market events are later expansion, not implemented menu promises.

The host checkpoints changed state approximately every 30 seconds, on explicit
Profile → A, and on normal Home exit. A/B files are versioned, checksummed and
read back after writing. A failed save is visible in Profile. Sudden power loss
can roll back changes since the last successful checkpoint. Leave through Home
before switching off. Selling after a rug remains funded because curve reserves
are separate from withdrawable fee treasuries.

## Development and verification

### ESP-IDF toolchain (optional, for replacement firmware development)

The official Custom Flash guide pins ESP-IDF 5.5.3. Install the ESP32-C3
toolchain inside this checkout (downloads are excluded from Git):

```sh
bash tools/setup_esp_idf.sh
bash tools/idf.sh --version
```

`tools/idf.sh` activates that installation and forwards arguments to `idf.py`
in your current directory. The standalone C++ firmware project is in `firmware/`.
The online IDE's **Push** uploads Lua app files; it does not flash the native
firmware. See [`firmware/README.md`](../firmware/README.md) for the native workflow.

### App tests and simulator

```sh
make test
make package
python3 simulator/server.py
```

Open `http://127.0.0.1:8765` for two virtual badges running the same app Lua.
This is a development simulator; its widget mock is not LVGL and its simulated
radio latency is not a physical measurement. It deliberately supports packet
loss testing. Source dependencies for tests are vendored Lua 5.4.8, C compiler,
make, and Python 3. The badge itself needs none of those tools.

The optional 32-bit ABI heap test uses a local
[WASI SDK](https://github.com/WebAssembly/wasi-sdk):

```sh
python3 tools/build_quota_wasm.py /path/to/wasi-sdk
node tools/quota_wasm.mjs
```

See [`docs/verification.md`](verification.md) for measured results and
remaining physical-device gates. Desktop heap tests include a mock SDK and
must not be presented as actual ESP32 free heap. The manifest's 96 KiB limit is
a ceiling, not a reservation or proof that enough system RAM is available.

See [`docs/stock-runtime.md`](stock-runtime.md) for the C/Lua boundary and
[`docs/protocol.md`](protocol.md) for the retained Lua wire/state formats.
The current native implementation plan is in
[`docs/architecture.md`](architecture.md).
