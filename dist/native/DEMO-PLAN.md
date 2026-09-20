# Badge Market — three-minute demo

**The story: solve a puzzle, earn game SOL, launch a coin, and trade with the
person beside you.** Show two real badges throughout. Let the screens prove
each step; use the voiceover to explain why it matters.

For this recording, power both badges over USB. The battery brownout is still
being investigated. Say **“USB supplies power; the badges exchange game data
wirelessly.”** Leave the cables visible in at least one wide shot. A laptop is
not acting as the market server.

## The finished cut — exactly 3:00

| Time | Picture and actions | Suggested voiceover |
| --- | --- | --- |
| **0:00–0:12** | Two badges side by side. Quick close-up of a coin, a word grid, then both badges. Put “Badge Market” on screen. | “We turned these conference badges into a shared trading game. Play to earn game money, launch a coin, and trade with the person beside you.” |
| **0:12–0:32** | Badge A: **Host nearby market**. Hold on its code. Badge B: **Join nearby market**, select that code, A. Show both matching dashboards. Keep the actual join in one continuous shot if it fits. | “One badge hosts the market; the other joins its code. They communicate directly over ESP-NOW, without an internet connection. USB supplies power; game data travels wirelessly.” |
| **0:32–0:50** | On the guest: **Games → MEMEWORD**. Show one cursor move and letter change, then a completed guess. Press A and hold on the tile reveal. Cut out most letter cycling. | “MEMEWORD is a five-letter puzzle built for the badge buttons. Green means the right position, amber means the letter belongs elsewhere, and gray means it is absent.” |
| **0:50–1:08** | Jump forward within that same recorded round. Show the final submitted guess, then the real result, reward, and new wallet balance. Keep reward and balance readable for several seconds. | “The host checks the submitted guesses and awards the result. That reward goes into your market wallet. A puzzle only pays once; replaying it is practice.” |
| **1:08–1:28** | Badge A: **Launch a coin**, choose a short symbol, select **Launch for 2 SOL**, A. Badge B: open **Market** and show that coin appearing. Cut name entry, preserve the launch and remote appearance. | “Now we create a coin for two SOL. It appears on the other badge. The host keeps the shared wallets, supply, and trades consistent.” |
| **1:28–1:52** | Badge B: open the coin, **Buy**, set quantity to 5, show Cost, A. Hold on **PURCHASED**. Open Portfolio and the holding. Open **Sell**, set quantity to 2, A; show **SOLD** and the remaining 3 tokens. | “My friend buys five tokens, checks the confirmation, and then sells two. Prices follow the coin's supply and trades include a fee. The receipts and portfolio show what actually changed.” |
| **1:52–2:10** | **Games → RUG RUN**. Show a lane change, an A jump over a low rug, and collecting gold SOL. Cut to the real result from the same run, if available. | “RUG RUN adds an arcade game: change lanes, jump over rugs, and collect SOL. The host replays the recorded inputs to validate the score before paying the reward.” |
| **2:10–2:36** | Both badges: **Games → RUG BOMB**. Host creates lobby; friend joins. Both Ready. Show **YOU HAVE THE RUG**, target selection, the recipient's button prompt, and confirmed ownership moving. Cut timer waiting; include explosion only if captured. | “RUG BOMB is the multiplayer game. Pass the rug to another player, who must answer a button challenge. The host confirms the transfer, and whoever holds it when the hidden timer expires gets rugged.” |
| **2:36–2:52** | Back in the market, badge A opens its coin's **Creator controls → Pull the rug**. Show the warning and fresh three-second A hold. Badge B shows the closed coin and sells one of its remaining tokens. | “The coin creator can also pull the rug. It costs reputation and closes new buys, but holders can still sell because their reserves stay funded.” |
| **2:52–3:00** | Both badges in frame. Host uses **Home → Save and close**. End on the game title and repository name. | “A shared economy and three games, running on the badges. Save the market, then bring it back for the next group.” |

Treat the narration as a guide. Record it at a natural pace and trim to the
actual footage; don't rush through ten features while the viewer is reading.
The most important uninterrupted evidence is **joining, a confirmed trade on
the guest, and a real reward reaching its wallet**.

## MEMEWORD example you can explain clearly

An illustrative answer is **GOOSE**. These two words are in the badge dictionary:

| Guess | What the tiles mean if the answer is GOOSE |
| --- | --- |
| **STARE** | **S amber**, T gray, A gray, R gray, **E green** |
| **GOOSE** | All five green: solved |

Say: “The S is in the word but in a different spot. E is already correct. On the
next guess, all five letters match.” The two Os each occupy their own correct
position; duplicate letters are counted correctly.

This is an explanation, **not a fixed solution to every round**. The actual
answer comes from the current market puzzle. For camera footage, record the
real round and use its real guesses/result. Don't splice an illustrated GOOSE
grid into the live badge footage as if it were the current answer.

A fresh two-guess solve can pay **up to 8 SOL**, subject to the remaining reward
allowance. For example, a fresh 25 SOL wallet could become 33 SOL. Use the
number actually displayed, especially if you rehearsed that puzzle already.
If the recorded round ends without a solve, keep the real result and explain
the credited participation reward instead of presenting it as a win.

**Controls to demonstrate:** Left/Right chooses a tile; Up/Down changes its
letter (hold to scroll); B clears that tile; A submits all five letters.
Record one full round before editing. Cut typing, not the causal link between
that round, its result, and its wallet credit.

## Before you record

1. Put **native v0.3.1** on both badges. v0.3.0 uses the same game protocol, but
   both should get the latest power fixes. Stock/Lua and v0.2.x cannot join.
2. Use two known-good USB power connections and put the badges close together.
   Leave serial monitors and automatic button scripts closed while filming.
3. Do an actual two-badge join, buy/sell and Bomb pass rehearsal. Simulations
   alone do not establish that these interactions work on your two boards.
4. Check that the creator can launch a coin: enough SOL, no existing active
   coin, no rug cooldown. If the saved host already has a coin, use that coin
   and replace the launch footage/narration with “Here's our market's coin.”
   Do not erase your saves just to stage the demo.
5. Check that the guest has room in its four-slot portfolio and can afford
   five tokens. Use a smaller purchase if the quoted price is too high; change
   the narration and sell fewer tokens accordingly.
6. Record the MEMEWORD reward **before** replaying that puzzle. The next puzzle
   arrives every two market epochs, about four minutes of host uptime. A replay
   is practice and will not give you another first-time reward shot.
7. For Bomb, wait until both intended players are listed before the host marks
   Ready. Record an entire round, then cut the waiting time. Do the coin rug
   scene last: it permanently closes that coin's buys.
8. Save after useful takes. Keep a spare USB cable and fresh matched AAs handy.

## Camera and editing checklist

- Film landscape. Use an overhead two-badge shot for shared actions and a close
  shot for small text. Lock focus/exposure on the LCD; avoid blown-out whites.
- Take a short flicker test first. If you see bands, adjust the camera's shutter
  or anti-flicker setting before recording everything.
- Hold still for 2–3 seconds before and after each action. Get a clean view of
  host code, colored tiles, trade cost, receipt, reward and wallet.
- Record narration afterward. Keep button clicks quietly underneath it.
- Use simple cuts. Label A as **HOST** and B as **GUEST** consistently.
- For long typing or timer waits, use a short jump cut or “later in this round”
  caption. Use actual badge footage for successful transfers and rewards.
- If Bomb isn't reliable in rehearsal, replace its 26 seconds with a longer
  uninterrupted two-badge trade and a save/rejoin. State that group play needs
  more validation; do not describe an unrecorded pass as working.
- Export at **3:00 or slightly under**. Readable evidence is more useful than
  extra features squeezed into the final seconds.

Player reference: [Market guide](MARKET-README.md).
