# Badge Market — play with a friend

**One badge hosts. The other joins. Both trade in the same market.**

Use the native **Badge Market v0.3.1** firmware on both badges. The Lua app and
stock badge apps cannot join this version. No phone, Wi-Fi network, internet,
or pairing menu is needed. Start with the badges beside each other; USB power
is useful for your first test.

## Connect your two badges

1. **You:** choose **Host nearby market** with Up/Down, then press **A**.
2. Your dashboard shows **HOST** followed by a four-character code, such as
   **C80C**. Tell your friend that code.
3. **Your friend:** choose **Join nearby market**, then press **A**.
4. Wait a few seconds. Highlight **HOST C80C** (using your actual code) and
   press **A**. Wait for the market dashboard to appear.
5. The friend's dashboard shows **MARKET C80C**. You are now in the same market.

Keep the host badge powered on and inside its market. The host can browse,
trade, or play games while the friend plays. Closing the host's market
disconnects the other badge.

New players start with **25 SOL of game money**. A restored market keeps its
previous wallets and coins, so your balance may be different.

## Create the first coin

Either player can do this:

1. Open **Launch a coin** from the dashboard. An empty **Market** also offers
   **Launch first coin**.
2. Highlight a letter and press **A** to edit it. **Up/Down** changes the letter;
   **A** finishes that letter. **B** cancels that letter's edit.
3. Choose **Launch for 2 SOL** and press **A**.
4. Open **Market** on the other badge. The new coin appears after the next sync.

Each player can have one active coin. Creating a coin costs 2 SOL; it does not
give you a starting pile of its tokens. You can buy your own coin afterward.

## Buy and sell

1. Open **Market**, highlight a coin, and press **A**.
2. On the coin screen, **A** opens Buy; **B** opens Sell. Up/Down also selects
   Buy, Sell, Creator controls, or Back; A activates the highlighted action.
3. In the confirmation screen, **Up/Down** changes the quantity. **Cost** is
   what you pay to buy; **Receive** is what you get by selling, after fees.
4. **A confirms the trade; B cancels.** On a guest, wait for the host's result.
   A changed price can require a fresh quote and confirmation.
5. Look for **PURCHASED** or **SOLD** and the receipt. Open **Portfolio** to see
   holdings, their current sell value, and gain/loss where cost history is known.
   Select a holding with A to trade it.

You need enough SOL to buy and enough tokens to sell. There are four portfolio
slots. Prices rise as supply grows and fall as tokens are sold, so buying and
immediately selling usually loses a little to fees. A friend's trade can also
change the price before your request reaches the host.

**Try this together:** create one coin, have your friend buy one token, then
open their Portfolio and sell it. Watch their token count and wallet change.

## Buttons and leaving

| Button | What it does |
| --- | --- |
| Up / Down | Move through menu options |
| A | Select the highlighted option |
| B | Back in menus; Sell on coin detail; Cancel in a trade confirmation; clear letter in MEMEWORD |
| Home | Open the leave prompt from anywhere in a market |
| Start | Pause/options in games; return to the dashboard elsewhere |

The leave prompt starts on **Keep playing**. Choose **Save and close** (host),
**Save and leave** (solo), or **Leave market** (guest), then press **A** to return
to the Solo / Host / Join menu. **B** cancels the prompt. A failed save keeps the
host/solo market open so you can retry.

## Games — earn fake SOL

Choose **Games** from the dashboard. Up/Down chooses one of three games; A opens
it. Results show the actual reward and wallet. **A plays again; B returns to
Games.** All SOL is fictional game money, usable only in this market.

### MEMEWORD

Guess a five-letter word in six tries. **Left/Right** moves the cursor;
**Up/Down** cycles its letter (hold to scroll faster); **B** clears that letter;
**A** submits. Unknown/incomplete words do not consume a guess. Green/check means
correct position, amber/dot means present elsewhere, and dim gray means absent.

Solving in 1–6 guesses awards up to **10 / 8 / 6.5 / 5 / 3.5 / 2 SOL**;
six unsuccessful guesses award up to **0.75 SOL**. The same puzzle pays once.
Replays are practice. A new round puzzle becomes available every two market
clock epochs (about four minutes while the host remains powered on); this is
an offline puzzle schedule, not a real-world daily clock.

### RUG RUN

**Left/Right** changes lanes; **A** jumps over low rugs. Tall red candles require
a lane change. Collect gold SOL icons, keep your combo, and use **S** shield,
**M** magnet, and **R** rocket pickups. Three hearts give you room to recover.
A run ends at zero hearts or 40 seconds. **Start** pauses with Resume/Quit.

Rewards depend on score, up to **5 SOL per run** and **6 SOL per market epoch**.
The first three attempts in an epoch pay 100%, 50%, and 25%; later attempts are
practice. High scores still count. A guest's run is verified by the host before
its reward appears; remain nearby while the result is pending.

### RUG BOMB — 2–6 badges

1. Join the same nearby market using the instructions above.
2. Host: open **Games → RUG BOMB**, then press **A** to create the lobby.
3. Friends: open RUG BOMB and press **A** to join the host's advertised lobby.
4. Everyone presses **A** to mark Ready. The host should wait until all intended
   players are listed before marking Ready; the game starts when all are ready.
5. The holder sees **YOU HAVE THE RUG**. Press A, select a target with Up/Down,
   then A to attempt a pass. The target must press the prompted button quickly.
6. Ownership moves only after the host confirms. A missed/wrong response leaves
   the rug with the sender. An immediate return to the last sender is locked
   for two seconds. The hidden timer keeps running throughout.

Survivors receive up to **2 SOL**, plus **0.1 SOL per successful outgoing pass**
(maximum +0.5); the exploded holder gets up to **0.25 SOL** consolation. Bomb
rewards cap at 5 SOL/player/epoch. Host departure or too few connected players
cancels the round without rewards. Start opens options **without pausing**.

From Solo, RUG BOMB offers to save Solo and open your separate Host market.
To create your own lobby while visiting another host, leave that market and
choose Host from the first menu. There is no wallet transfer between markets.

The combined game-reward allowance is **20 SOL/player/epoch**. Reservations
and game-specific limits can reduce a result. Quitting gives no reward. Single
player games pause on their leave prompt; RUG BOMB continues. Keep playing is
always the default leave action. LEDs supplement the screen and can be disabled.

## Saving and reconnecting

- The **host saves everyone's market progress**. Choose **Profile / Settings →
  Save now**, or leave through **Save and close**. Changed state autosaves when due (about every 30 seconds),
  at a safe screen outside active gameplay. Pause or finish a game to allow saving.
- A guest can leave and rejoin the same host with the same badge to recover
  their wallet. A request already received by the host may still complete when
  the guest leaves; check the wallet after rejoining.
- To resume later, the same host chooses **Host nearby market** again. Friends
  should return to the first menu and **Join** again after a host restart.
- Solo has a separate save. Switching hosts does not transfer your wallet.
- Powering off can lose changes since the last save. Save before removing power.

## If something doesn't work

| What you see | What to try |
| --- | --- |
| No host listed | Check both badges use native v0.3.x (v0.3.1 recommended). v0.2.x and Lua releases are incompatible. Host must already be inside its market. Bring badges closer; back out of Join and try again. |
| Joining or trade pending | Keep both badges powered and nearby. The guest waits for the host; trades cannot complete offline. |
| Host out of range | Return near the host. If the host restarted, leave and join again. |
| Not enough funds / tokens | Lower the quantity or check your wallet and holdings. |
| Power Dropped / return to first menu | Install v0.3.1's calibration-storage and radio-power fixes, then retest on fresh matched AA batteries. Use USB power for the recorded demo until battery testing passes. Weak batteries or poor contacts can still brown out. |

**Creator controls** withdraw trading fees. **Pull the rug** closes new buys,
takes the remaining creator and community fees, and costs reputation; holders can still sell.
It has a separate warning and requires a fresh three-second A hold.

Need to install the firmware? See the [build and flash guide](README.md).

Recording a demo? See the [three-minute shot list and MEMEWORD example](DEMO-PLAN.md).
