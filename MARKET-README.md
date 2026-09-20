# Badge Market — play with a friend

**One badge hosts. The other joins. Both trade in the same market.**

Use the native **Badge Market v0.2.4** firmware on both badges. The Lua app and
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
trade, or play minigames while the friend plays. Closing the host's market
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
2. Choose **Quantity** to set how many tokens you want. **Up** adds one,
   **Down** removes one, and **A** finishes.
3. Highlight **Buy** or **Sell**. The displayed SOL amount is the total for
   that quantity, including the fee. Buy is what you pay; Sell is what you get.
4. Press **A** to submit. On a joined badge, wait for the host's result before
   making another trade. Simply highlighting an option does not trade.
5. Open **Portfolio** to see your tokens. Select a holding with **A** to trade it.

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
| B | Go back one screen; at the dashboard, open the leave prompt |
| Home | Open the leave prompt from anywhere in a market |
| Start | Return to the market dashboard without disconnecting |

The leave prompt starts on **Keep playing**. Choose **Save and close** (host),
**Save and leave** (solo), or **Leave market** (guest), then press **A** to return
to the Solo / Host / Join menu. **B** cancels the prompt. A failed save keeps the
host/solo market open so you can retry.

In the **Reaction Trader** minigame, B means SELL during a round. Use **Start**
for the dashboard or **Home** to leave. After the round, B returns to Minigames.

## Saving and reconnecting

- The **host saves everyone's market progress**. Choose **Profile / Settings →
  Save now**, or leave through **Save and close**. Changes also autosave about
  every 30 seconds.
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
| No host listed | Check both badges use the native firmware. Host must already be inside its market. Bring badges closer; back out of Join and try again. |
| Joining or trade pending | Keep both badges powered and nearby. The guest waits for the host; trades cannot complete offline. |
| Host out of range | Return near the host. If the host restarted, leave and join again. |
| Not enough funds / tokens | Lower the quantity or check your wallet and holdings. |
| Power Dropped / return to first menu | Try USB power or fresh matched AA batteries. The lower-power firmware helps, but weak batteries can still brown out. |

For more SOL, try **Minigames → Reaction Trader**: wait for the cue, then
**A = BUY, B = SELL, Up = HOLD**. Eight rounds can earn up to 2 SOL. Tilt Vault
and badge duels are practice/competition and do not award SOL.

**Creator controls** withdraw trading fees. **Pull the rug** closes new buys,
takes the remaining creator and community fees, and costs reputation; holders can still sell.
It has a separate warning and requires a fresh three-second A hold.

Need to install the firmware? See the [build and flash guide](firmware/README.md).
