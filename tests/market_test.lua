package.path='app/badge_market/?.lua;'..package.path
local E=require('market')
local C=require('codec')
local n=0
local function eq(a,b,why) n=n+1; assert(a==b,(why or 'assertion')..': '..tostring(a)..' ~= '..tostring(b)) end
local function act(m,p,op,id,data,t)
  local _,seq=E.wallet(m,p)
  return E.request(m,p,seq,op,id or 0,data or '',t or 100000)
end
local function bal(m,p) return (E.wallet(m,p)) end
local function info(m,c,p) return {E.info(m,c,p)} end
local function fresh()
  local m=E.new(123)
  local a=E.join(m,'AAAAAA'); local b=E.join(m,'BBBBBB')
  eq(act(m,a,1,0,'GOOSE'),0)
  return m,a,b
end
eq(E.cost(0,1),100); eq(E.cost(0,10),1225)
eq(E.cost(0,0),nil); eq(E.cost(0,-1),nil); eq(E.cost(10000,1),nil)
eq(E.cost(0,10000),250975000); eq(E.cost(0,1.5),nil)
for q=0,100 do for k=1,20 do eq(E.cost(q,k),E.cost(0,q+k)-(q>0 and E.cost(0,q) or 0),'curve symmetry') end end
local m,a,b=fresh()
eq(bal(m,a),23000); eq(bal(m,b),25000)
eq(act(m,b,2,1,string.pack('<I2',10)),0)
eq(bal(m,b),23750); eq(info(m,1,b)[8],10)
local _,seq=E.wallet(m,b)
eq(E.request(m,b,seq-1,2,1,string.pack('<I2',10),100000),0,'duplicate result')
eq(bal(m,b),23750,'duplicate not charged')
eq(E.request(m,b,seq-1,2,1,string.pack('<I2',11),100000),3,'conflicting replay')
eq(E.request(m,b,seq-2,2,1,string.pack('<I2',10),100000),3,'old request')
eq(act(m,b,3,1,string.pack('<I2',11)),7)
eq(act(m,b,3,1,string.pack('<I2',10)),0)
eq(bal(m,b),24950,'round trip loses only fees')
eq(info(m,1,b)[2],0); eq(info(m,1,b)[4],50)
eq(act(m,b,4,1,string.char(100)),9,'creator authorization')
eq(act(m,a,4,1,string.char(25)),0)
eq(bal(m,a),23006,'25 percent creator share')
eq(act(m,a,4,1,string.char(100)),0)
eq(bal(m,a),23050,'rug fee treasury only')
eq(info(m,1,b)[7],1)
eq(select(3,E.wallet(m,a)),40); eq(select(4,E.wallet(m,a)),1)
eq(act(m,a,1,0,'COPE'),10,'rug cooldown')
eq(act(m,b,2,1,string.pack('<I2',1)),8,'rug stops buying')
m,a,b=fresh()
act(m,b,2,1,string.pack('<I2',50))
local before=bal(m,b)
E.epoch(m); eq(bal(m,b),before,'new holdings wait full epoch')
E.epoch(m); assert(bal(m,b)>before,'eligible holder rewarded')
act(m,a,4,1,string.char(100))
eq(act(m,b,3,1,string.pack('<I2',50)),0,'sells solvent after rug')
eq(info(m,1,b)[2],0)
local bytes=E.encode(m); local restored=assert(E.decode(bytes))
eq(bal(restored,b),bal(m,b)); eq(restored.gen,1)
for i=1,#bytes do
  local broken=bytes:sub(1,i-1)..string.char(bytes:byte(i)~1)..bytes:sub(i+1)
  eq(E.decode(broken),nil,'snapshot corruption')
end
eq(E.decode(bytes:sub(1,-2)),nil); eq(E.decode(''),nil)
local frame=C.frame(1,123,456,'payload'); eq(#frame,19)
local k,id,s,data=C.parse(frame); eq(k,1); eq(id,123); eq(s,456); eq(data,'payload')
eq(C.frame(1,1,1,string.rep('a',33)),nil)
eq(C.parse(frame:sub(1,-2)),nil)
eq(#C.mac('AA:3A:BB:CC:DD:EE'),6,'MAC containing colon byte')
-- Host-issued game ticket + bounded transcript, replay-proof reward.
m,a,b=fresh()
local code,_,ticket=act(m,b,5,0,'',1000); eq(code,0); assert(ticket>0)
local keys,times,total={},{},800+7*180
for i=1,8 do local cmd,delay=C.command(ticket,i); keys[i]=cmd; times[i]=15; total=total+delay+300 end
local proof=string.char(table.unpack(keys))..string.char(table.unpack(times))
eq(act(m,b,6,0,proof,1000+total),0); eq(bal(m,b),27000)
eq(act(m,b,6,0,proof,20000),14,'ticket consumed')
eq(act(m,b,5,0,'',19000),13,'cooldown')
for round=2,3 do
 local at=round*22000
 local code,_,ticket=act(m,b,5,0,'',at);eq(code,0)
 local keys,times,total={},{},800+7*180
 for i=1,8 do local key,delay=C.command(ticket,i);keys[i]=key;times[i]=15;total=total+delay+300 end
 eq(act(m,b,6,0,string.char(table.unpack(keys))..string.char(table.unpack(times)),at+total),0)
end
eq(bal(m,b),31000,'6000 per-epoch reward cap')
eq(act(m,b,5,0,'',100000),15,'fourth full-reward game denied')
E.epoch(m);eq(act(m,b,5,0,'',120000),0,'new epoch replenishes budget')
-- Capacity and thousands of randomized trades; supply/reserve invariant checked by decoder.
m,a,b=fresh()
for i=3,12 do assert(E.join(m,string.format('%06d',i))) end
eq(E.join(m,'13FULL'),nil)
math.randomseed(7341)
for i=1,3000 do
  local who=math.random(1,12)
  act(m,who,math.random(2,3),1,string.pack('<I2',math.random(1,20)),i*1000)
  if i%100==0 then assert(E.decode(E.encode(m)),'accounting invariant'); n=n+1 end
end
-- 50 launches across independent bounded markets (full markets never evict balances).
for round=1,5 do
  local w=E.new(round)
  for j=1,12 do local p=E.join(w,string.format('%06d',j)); eq(act(w,p,1,0,'C'..j),0) end
  eq(#w.c,12)
end
print('market: '..n..' checks passed')
