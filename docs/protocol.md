# Economy, wire, persistence

`market.lua` is a pure authority module usable without badge hardware.
It owns packed player/coin strings; callers never set arbitrary balances.
All arithmetic uses Lua integers. Values fit the explicit wire/storage ranges.
GAME_SOL balances are capped at 1,000,000,000 units.

## Economy

- 1 displayed SOL = 1000 units; initial grant = 25,000; launch fee = 2,000.
- Linear curve base 100, slope 5, max supply 10,000.
- Buy cost: `n*100 + 5*n*(2*q+n-1)//2`.
- Sell gross: the same interval starting at `q-n`.
- Fee: `(gross+49)//50` (2%, rounded up); half rounded down to creator fees,
  rest to community fees. Sell-after-rug fee = zero.
- Curve reserve must equal `cost(0,supply)` at every checkpoint. Aggregate
  holdings must equal circulating supply. Decoder rejects violations.
- 120-second foreground host epochs. Newly acquired quantity first becomes
  eligible at the next boundary and is paid after a full subsequent epoch.
  Additional buys do not retroactively qualify. Selling reduces eligible
  quantity and resets hold streak.
- Half the community treasury is distributed proportionally to qualifying
  quantity, weighted 100/110/120/125% for completed hold streak. Integer dust
  remains in treasury. No reward is paid once rugged.
- First distinct holder adds 10 meme power; a 12-bit bitset prevents rebuy
  farming. Meme power decays 5% per epoch. A live funded coin gains its creator
  one reputation point every three epochs, capped at 100.
- 10/25/50% withdrawals affect creator fees only. A full rug drains fee
  treasuries, marks rugged, zeros meme power, drops reputation by 35, and adds
  three epochs of launch cooldown. Curve reserve remains redeemable.

## Frames

Header: `BM1` (3 bytes), type:u8, market:u16, sequence:u16. Payload follows;
final CRC32:u32 covers header and payload. Explicit little endian. Max 44 bytes.
This is inside the stock firmware's own LUA1 framing. CRC is not authentication.

| Type | Purpose | Payload | Full bytes |
|---|---|---|---:|
| 0 | Host beacon | padded name:8, epoch:u16 | 22 |
| 1 | Join / read wallet | destination host MAC:6 | 18 |
| 2 | Read-wallet reply | destination:6, player:u8, code:u8, balance:u32, next:u16, rep:u8, rugs:u8, epoch:u16, coin:u8, ticket:u32, coinCount:u8 | 36 |
| 3 | Detail query | host:6, player:u8, coin:u8 | 20 |
| 4 | Detail reply | recipient:6, coin:u8, symbol:5, supply:u16, creator:u8, fees:u32, meme:u16, holders:u8, rugged:u8, owned:u16 | 37 |
| 5 | Mutation | host:6, player:u8, operation:u8, argument:u16, data:0..16 | 22..38 |
| 6 | Rug event | coin:u8 | 13 |
| 7 | Nearby trader | player:u8 | 13 |
| 8 | Mutation reply | same payload as type 2; distinct type prevents stale reads acknowledging writes | 36 |
| 20 | Duel challenge | recipient:6, seed:u32 | 22 |
| 21..23 | Accept / start / ACK | recipient:6 | 18 |
| 24 | Duel result | recipient:6, reactionMs:u16 (65535 = false start) | 20 |
| 25..26 | Result ACK / cancel | recipient:6 | 18 |

Market mutations: 1 create, 2 buy, 3 sell, 4 withdraw/rug, 5 issue game ticket,
6 submit game transcript. The host matches player slot to observed source MAC.
Results are cached with the last request fingerprint. Exact retry returns the
previous result; conflicting or older sequences cannot mutate balances.
Sequence 65535 is rejected rather than wrapping. Join restores canonical next
sequence after client reboot. One client request is in flight, at most four
automatic sends; timeout retains unresolved writes for explicit Start retry.

Reaction transcripts contain eight key bytes and eight response-time bytes
(20 ms units). Commands are generated from a host-issued seed. Host checks
size, command sequence, response range, aggregate elapsed time, one-use ticket,
cooldown, expiry, and epoch budget. It cannot prevent a modified client from
fabricating a plausible transcript. Duels intentionally award no SOL and use
local reaction duration, so radio transit is excluded from the comparison.

## Binary state

Snapshot header: magic `BMS1`, generation:u32, market:u16, epoch:u16,
players:u8, coins:u8 (14 bytes). Then players, then coins, then CRC32:u32.
Player: 40-byte fixed header plus four 7-byte holdings = **68 bytes**.
Coin: **27 bytes**. At capacity: `18 + 12*68 + 16*27 = 1266` bytes.
Storage never serializes Lua tables, code or pointers. Saves have no wall clock.
Game tickets are cleared on restoration. Transaction sequence/result remains.

Slots `appdata/host_a`, `host_b` and separate `solo_a`, `solo_b`. New generation
alternates slot. Readback must exactly match before live generation advances.
On boot both slots undergo CRC, length, range, reserve and supply validation;
choose the highest valid generation. No valid slot means a fresh local market.
There is no per-trade durability claim; periodic snapshots can roll back a
short interval after abrupt power loss. Clients never promote cached balances
to a new authority.
