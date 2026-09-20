local T=dofile('tests/sandbox.lua')
local function see(d,s) assert(T.text(d):find(s,1,true),'Missing screen text: '..s..'\n'..T.text(d)) end
local a=T.new('01:02:03:04:05:06')
T.press(a,'DOWN');T.press(a,'A');T.advance(200)
see(a,'25.000 SOL')
T.press(a,'DOWN');T.press(a,'A');T.advance(100)
assert(a.modules.session.count==1)
T.press(a,'START');T.press(a,'A');T.press(a,'A');T.press(a,'A');T.advance(100)
assert(a.modules.session.balance==22898,'buy price')
T.press(a,'B');T.advance(100);assert(a.modules.session.balance==22996,'sell fee')
local b=T.new('01:02:03:04:05:07')
T.press(b,'DOWN');T.press(b,'DOWN');T.press(b,'A');T.advance(2500)
see(b,'Market #')
T.drop=1;T.duplicate=true;T.press(b,'A');T.advance(3500)
see(b,'25.000 SOL');assert(b.modules.session.id==2)
T.press(b,'A');T.advance(2500);T.press(b,'A');T.advance(100)
see(b,'$GOOSE');T.press(b,'A');T.advance(2500)
assert(b.modules.session.balance==24898,'remote buy once despite duplicate frames')
assert(a.modules.session.detail(1)[6]==1)
-- Response loss: identical request retries must not double spend.
T.press(b,'RIGHT');T.drop=2;T.press(b,'A');T.advance(6000)
if b.modules.session.busy() then T.press(b,'START');T.advance(4500) end
assert(b.modules.session.balance==24678,'retry buys exactly two tokens')
-- Cooperative duel handshake and result exchange with duplicate frames.
T.press(a,'START');T.press(a,'LEFT');T.advance(2500);T.press(a,'A');T.advance(500)
see(b,'DUEL INVITATION');T.press(b,'A');T.advance(800)
T.advance(4000)
T.press(a,'A');T.advance(200);T.press(b,'A');T.advance(1200)
see(a,'YOU OUT-HONKED THEM');see(b,'YOU GOT HONKED')
-- Rug requires actual held state, cancels on release.
T.press(a,'A');T.press(a,'A');T.advance(200);T.press(a,'A');T.advance(200);T.press(a,'DOWN')
for _=1,3 do T.press(a,'DOWN') end
T.press(a,'A');T.advance(100);see(a,'YOUR REP: COOKED')
T.press(a,'START');T.advance(3200);assert(a.modules.session.detail(1)[7]==0,'released Start cannot rug')
a.held[9]=true;a.env.on_button(9,1);T.advance(3100);a.held[9]=false;a.env.on_button(9,2)
T.advance(1000);see(b,'HOLD THIS L.');assert(a.modules.session.rep==40)
-- Failed inactive snapshot never overwrites the old slot.
local s=a.modules.session;s.checkpoint();assert(s.save=='Saved')
a.failwrite=true;s.act(3,1,string.pack('<I2',1));s.checkpoint();assert(s.save=='SAVE FAILED')
a.failwrite=false;s.checkpoint();assert(s.save=='Saved')
-- Repeated navigation uses exactly the same native widget pool.
local count=#a.widgets
T.advance(9000)
for _=1,300 do
 T.press(a,'START');T.press(a,'A');T.press(a,'A');T.press(a,'START');T.press(a,'B');T.press(a,'B')
 T.advance(100)
end
assert(#a.widgets==count and count==12,'stable widget pool')
a.env.on_exit();b.env.on_exit()
print('app: two badges, lost/duplicate packets, duel, rug, saves, 300 scene cycles passed')
