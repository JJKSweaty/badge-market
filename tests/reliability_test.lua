package.path='app/badge_market/?.lua;'..package.path
local C=require('codec')
local E=require('market')
local T=dofile('tests/sandbox.lua')
local a=T.new('01:02:03:04:05:06')
T.press(a,'DOWN');T.press(a,'A');T.advance(100)
T.press(a,'DOWN');T.press(a,'A');T.advance(100)
local b=T.new('01:02:03:04:05:07')
T.press(b,'DOWN');T.press(b,'DOWN');T.press(b,'A');T.advance(2500)
T.press(b,'A');T.advance(2500)
local s=b.modules.session;assert(not s.busy())
local seq=s.seq
assert(s.act(2,1,string.pack('<I2',1)))
local stale=C.frame(2,s.market,seq,string.pack('<c6I1I1I4I2I1I1I2I1I4I1',
 C.mac(b.mac),s.id,0,25000,seq,75,0,1,0,0,1))
b.recv(a.mac,-45,stale)
assert(s.busy(),'a delayed read response must not ACK a mutation')
T.advance(1500);assert(s.balance==24898 and not s.busy())
-- Offline RPC is retained for explicit retry; no local wallet mutation.
a.radio=false
assert(s.act(2,1,string.pack('<I2',1)));T.advance(5500)
assert(s.busy() and s.balance==24898)
a.radio=true;s.retry();T.advance(1500)
assert(not s.busy() and s.balance==24790)
-- Corrupt newest slot and restart: valid older generation wins.
local host=a.modules.session
host.checkpoint()
local old
for _,data in pairs(a.files) do local m=E.decode(data);if m and (not old or m.gen>old.gen) then old=m end end
assert(old)
host.act(2,1,string.pack('<I2',1));host.checkpoint()
for path,data in pairs(a.files) do local m=E.decode(data);if m and m.gen>old.gen then a.files[path]=data:sub(1,-2)..'X' end end
local restored=T.new(a.mac,a.files);T.press(restored,'DOWN');T.press(restored,'A');T.advance(100)
assert(restored.modules.session.balance==E.wallet(old,1),'A/B fallback restores earlier balance')
-- Random malformed network input is ignored without exceptions.
math.randomseed(1287)
for _=1,10000 do
 local bytes={};for i=1,math.random(0,60) do bytes[i]=string.char(math.random(0,255)) end
 b.recv(a.mac,-45,table.concat(bytes))
end
for _=1,1000 do
 local bytes={};for i=1,math.random(0,26) do bytes[i]=string.char(math.random(0,255)) end
 local frame=C.frame(math.random(0,26),s.market,math.random(0,65535),C.mac(a.mac)..table.concat(bytes))
 if frame then a.recv(b.mac,-45,frame);b.recv(a.mac,-45,frame) end
end
print('reliability: stale ACK, host outage, retry, A/B fallback, 11,000 malformed frames passed')
