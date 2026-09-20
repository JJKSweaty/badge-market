local T=require('copy')
-- One in-flight RPC, bounded discovery/cache, no gameplay in the radio handler.
local C=require('codec')
local S={mode='offline',balance=0,rep=75,rugs=0,epoch=1,seq=1,id=0,market=0,
 coins={},peers={},players={},count=0,changed=true,status=T(162),ticket=0,save=T(163)}
local E,world,mac,host,pending,radio
local detailScratch={}
local rugCoin,rugTries,rugAt,rugSeen=0,0,0,0
local now,beacon,poll,epochAt,saveAt,socialAt=0,0,0,0,0,0
local pack,unpack=string.pack,string.unpack
local function refresh()
  S.changed=true
  if world then
    S.balance,S.seq,S.rep,S.rugs=E.wallet(world,S.id)
    S.epoch=world.epoch; S.count=#world.c
  end
end
local function message(code)
  S.status=code==0 and T(164) or (E and E.error(code) or
    ({[1]=T(165),[3]=T(166),[4]=T(167),[6]=T(168),
    [7]=T(169),[8]=T(170),[10]=T(171),
    [11]=T(172),[12]=T(173),[13]=T(174),
    [14]=T(175),[15]=T(176)})[code] or T(177))
  S.changed=true
end
local function transmit(s) if radio and s then return badge.radio.send(s) end return false end
local function rug(id)
  if id<1 or id>16 then return end
  if rugSeen & (1<<id)==0 then rugSeen=rugSeen | (1<<id); S.rug=id; S.changed=true end
  if world then rugCoin=id; rugTries=3; rugAt=now end
end
local function response(pid,seq,code,coin,ticket,kind)
  local bal,nextseq,rep,rugs=E.wallet(world,pid)
  return C.frame(kind or 2,world.id,seq,pack('<c6I1I1I4I2I1I1I2I1I4I1',
    world.p[pid]:sub(1,6),pid,code,bal,nextseq,rep,rugs,world.epoch,coin or 0,ticket or 0,#world.c))
end
function S.detail(id)
  if id<1 or id>S.count then return nil end
  if world then
    detailScratch[1],detailScratch[2],detailScratch[3],detailScratch[4],
      detailScratch[5],detailScratch[6],detailScratch[7],detailScratch[8]=E.info(world,id,S.id)
    return detailScratch -- borrowed until the next detail() call
  end
  return S.coins[id]
end
function S.fetch(id)
  if world or pending or not host or id<1 or id>S.count then return end
  pending={C.frame(3,S.market,S.seq,host..pack('<I1I1',S.id,id)),0,now,3,id}
end
local function hostReceive(other,kind,market,seq,p)
  if world and S.mode=='host' and market==world.id then
    if #p<6 or p:sub(1,6)~=mac then return end
    if kind==1 and #p==6 then
      local pid=E.join(world,other)
      if pid then transmit(response(pid,seq,0,0,0)) end
    elseif (kind==3 and #p==8) or (kind==5 and #p>=10 and #p<=26) then
      local pid=p:byte(7)
      if not world.p[pid] or world.p[pid]:sub(1,6)~=other then return end
      if kind==3 then
        local id=p:byte(8); local sym,q,creator,fees,meme,holders,rug,owned=E.info(world,id,pid)
        if sym then transmit(C.frame(4,market,seq,pack('<c6I1c5I2I1I4I2I1I1I2',other,id,sym,q,creator,fees,meme,holders,rug,owned))) end
      else
        local op,a=unpack('<I1I2',p,8)
        local code,id,ticket=E.request(world,pid,seq,op,a,p:sub(11),now)
        transmit(response(pid,seq,code,id,ticket,8)); refresh()
        if code==0 and op==4 and p:byte(11)==100 then
          rug(id)
        end
      end
    end
    return
  end
end
local function clientReceive(other,kind,market,seq,p)
  if S.mode~='client' or other~=host or market~=S.market then return end
  if kind==6 and #p==1 then rug(p:byte(1)); return end
  if not pending or seq~=S.seq or p:sub(1,6)~=mac then return end
  if ((kind==2 and pending[4]==1) or (kind==8 and pending[4]==5)) and #p==24 then
    local _,id,code,bal,nextseq,rep,rugs,ep,coin,ticket,count=unpack('<c6I1I1I4I2I1I1I2I1I4I1',p)
    if id<1 or id>12 or count>16 or nextseq<1 then return end
    S.id=id; S.balance=bal; S.seq=nextseq; S.rep=rep; S.rugs=rugs; S.epoch=ep
    S.ticket=ticket; S.count=count; S.lastcoin=coin; pending=nil; message(code); S.lastseen=now
    S.coins={}; S.start=nil; S.join=nil
  elseif kind==4 and #p==25 and pending[4]==3 and p:byte(7)==pending[5] then
    local _,id,sym,q,creator,fees,meme,holders,rug,owned=unpack('<c6I1c5I2I1I4I2I1I1I2',p)
    if q>10000 or creator>12 or rug>1 then return end
    -- Eight-entry cache, deterministic oldest-index eviction.
    local n=0; for _ in pairs(S.coins) do n=n+1 end
    if n>=8 and not S.coins[id] then for k in pairs(S.coins) do S.coins[k]=nil; break end end
    S.coins[id]={sym:gsub('%z',''),q,creator,fees,meme,holders,rug,owned}
    pending=nil; S.changed=true; S.lastseen=now
  end
end
local function receive(from,rssi,data)
  local kind,market,seq,p=C.parse(data); if not kind then return end
  local other=C.mac(from); if not other or other==mac then return end
  if market==S.market and kind==7 and #p==1 and S.mode~='offline' then
    local index
    for i=1,#S.players do if S.players[i][1]==other then index=i end end
    if not index then
      index=#S.players+1
      if index>8 then index=1; for i=2,8 do if S.players[i][3]<S.players[index][3] then index=i end end end
    end
    S.players[index]={other,p:byte(1),now}; S.changed=true; return
  end
  if kind>=20 and market==S.market and #p>=6 and p:sub(1,6)==mac then
    if S.listener then S.listener(other,kind,seq,p:sub(7),now) end
    return
  end
  if kind==0 and #p==10 and S.mode=='offline' then
    local i
    for j=1,#S.peers do if S.peers[j][1]==other then i=j end end
    if not i then
      i=#S.peers+1
      if i>8 then i=1; for j=2,8 do if S.peers[j][4]<S.peers[i][4] then i=j end end end
    end
    S.peers[i]={other,market,p:sub(1,8):gsub('%z',''),now,rssi}; S.changed=true
    return
  end
  if hostReceive and world then hostReceive(other,kind,market,seq,p)
  elseif clientReceive then clientReceive(other,kind,market,seq,p) end
end
function S.connect()
  if radio then return true end
  radio=badge.radio.enable()
  if not radio then S.status=T(178); S.changed=true; return false end
  mac=C.mac(badge.radio.mac())
  badge.radio.on_recv(receive)
  return mac~=nil
end
function S.start(mode)
  if S.mode~='offline' then return false end
  if mode=='host' and not S.connect() then return false end
  local stats=badge.sys.stats()
  if stats.lua_used+28672>stats.lua_limit or stats.free_heap<65536 then
    S.status='Memory busy. Join another host.'; S.changed=true; return false
  end
  E=require('market')
  local a=E.decode(badge.fs.read('appdata/'..mode..'_a'))
  local b=E.decode(badge.fs.read('appdata/'..mode..'_b'))
  world=(a and b) and (a.gen>b.gen and a or b) or a or b
  world=world or E.new(badge.sys.random(65534)+1)
  S.id=E.join(world,mode=='host' and mac or 'SOLO00')
  if not S.id then S.status=T(179); world=nil; return false end
  S.mode=mode; S.market=world.id; epochAt=now+120000; saveAt=now+30000
  S.save=world.gen>0 and T(180) or T(181); refresh(); S.status=T(182)
  S.start=nil; S.join=nil -- one-time setup code can be collected after mode selection
  E.decode=nil; E.new=nil; clientReceive=nil; S.peers={} -- release boot/client-only code
  return true
end
function S.join(i)
  if S.mode~='offline' or not S.peers[i] or now-S.peers[i][4]>8000 then return end
  host=S.peers[i][1]; S.market=S.peers[i][2]; S.mode='client'; S.seq=1; hostReceive=nil; S.peers={}
  pending={C.frame(1,S.market,S.seq,host),0,now,1}; S.status=T(183); S.changed=true
end
function S.act(op,a,b)
  if pending then S.status=T(184); S.changed=true; return false end
  S.ticket=0
  if world then
    local code,id,ticket=E.request(world,S.id,S.seq,op,a or 0,b or '',now)
    S.lastcoin=id; S.ticket=ticket or 0; message(code); refresh()
    if code==0 and op==4 and b==string.char(100) then
      rug(id)
    end
    return code==0
  elseif S.mode=='client' and S.id>0 then
    pending={C.frame(5,S.market,S.seq,host..pack('<I1I1I2',S.id,op,a or 0)..(b or '')),0,now,5}
    S.status=T(185); S.changed=true; return true
  end
  S.status=T(186); S.changed=true; return false
end
function S.retry()
  if pending then pending[2]=0; pending[3]=now; S.status=T(187); S.changed=true end
end
function S.busy() return pending~=nil end
function S.send(kind,target,seq,data)
  return transmit(C.frame(kind,S.market,seq,target..(data or '')))
end
function S.checkpoint()
  if not world or not world.dirty then return end
  local data=E.encode(world); local path='appdata/'..S.mode..((world.gen+1)%2==0 and '_a' or '_b')
  local ok=badge.fs.write(path,data)
  if ok and badge.fs.read(path)==data then
    world.gen=world.gen+1; world.dirty=false; S.save=T(188)
  else S.save=T(189) end
  S.changed=true
end
function S.tick(t)
  now=t
  if rugTries>0 and now>=rugAt then
    transmit(C.frame(6,S.market,S.seq,string.char(rugCoin))); rugTries=rugTries-1; rugAt=now+700
  end
  if pending and now>=pending[3] and pending[2]<4 then
    transmit(pending[1]); pending[2]=pending[2]+1; pending[3]=now+1000
  elseif pending and pending[2]==4 and now>=pending[3] then
    S.status=T(190); S.changed=true; pending[2]=5
    if pending[4]==3 then pending=nil end
  end
  if world and now>=epochAt then E.epoch(world); epochAt=now+120000; refresh() end
  if world and now>=saveAt then S.checkpoint(); saveAt=now+30000 end
  if S.mode=='host' and now>=beacon then
    transmit(C.frame(0,S.market,0,pack('<c8I2',T(191),S.epoch)))
    beacon=now+1800+badge.sys.random(400)
  elseif S.mode=='client' and not pending and now>=poll then
    pending={C.frame(1,S.market,S.seq,host),0,now,1}; poll=now+6000
  end
  if radio and S.id>0 and now>=socialAt then
    transmit(C.frame(7,S.market,0,string.char(S.id))); socialAt=now+2200+badge.sys.random(400)
  end
end
function S.stop()
  S.checkpoint()
  if radio then badge.radio.on_recv(nil); badge.radio.disable() end
end
return S
