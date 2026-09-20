local T=require('copy')
-- Canonical authority. Packed records keep inactive players/coins off Lua tables.
local C = require('codec')
local M = { MAX_PLAYERS=12, MAX_COINS=16, HOLDINGS=4 }
local pack, unpack = string.pack, string.unpack
local PF = '<c6I4I2I2I2I2I1I1I2I2I4I4I4I4'
-- MAC,balance,nextSeq,rep,rugs,cooldown,lastCode,lastCoin,lastSeq,budget,hash,ticket,lastGame
local CF = '<I1I2I4I4I4I2I2I2I1c5'
-- creator,supply,reserve,creatorFee,communityFee,meme,seenBits,createdEpoch,rug,symbol
local HF = '<I1I2I2I2' -- coin,quantity,eligible,startEpoch
local PS = string.packsize(PF)
local CS = string.packsize(CF)
local empty = pack(HF,0,0,0,0):rep(4)
function M.error(code) return T(code>=0 and code<=16 and (192+code) or 177) end
local function player(m, id)
  local s=m.p[id]; if not s then return nil end
  local t={unpack(PF,s)}; t[15]=s:sub(PS+1); return t
end
local function putp(m,id,p)
  m.p[id]=pack(PF,table.unpack(p,1,14))..p[15]; m.dirty=true
end
local function coin(m,id)
  if not m.c[id] then return nil end
  return {unpack(CF,m.c[id])}
end
local function putc(m,id,c) m.c[id]=pack(CF,table.unpack(c,1,10)); m.dirty=true end
local function holding_data(data,id,offset)
  local free
  for i=1,4 do
    local a,b,c,d=unpack(HF,data,offset+(i-1)*7+1)
    if a==id then return i,b,c,d end
    if a==0 then free=i end
  end
  return free,0,0,0
end
local function holding(p,id) return holding_data(p[15],id,0) end
local function puth(p,i,id,q,e,start)
  local pos=(i-1)*7
  p[15]=p[15]:sub(1,pos)..pack(HF,q>0 and id or 0,q,e,start)..p[15]:sub(pos+8)
end
function M.new(id) return {id=id,epoch=1,p={},c={},dirty=true,gen=0} end
function M.join(m,mac)
  if #mac~=6 then return nil end
  for i=1,#m.p do if m.p[i]:sub(1,6)==mac then return i end end
  if #m.p>=M.MAX_PLAYERS then return nil end
  local id=#m.p+1
  putp(m,id,{mac,25000,1,75,0,0,0,0,0,0,0,0,0,0,empty})
  return id
end
function M.wallet(m,id)
  local p=player(m,id); if not p then return end
  return p[2],p[3],p[4],p[5],p[15],p[10]
end
function M.info(m,id,who)
  if not m.c[id] then return end
  local creator,supply,_,cf,hf,meme,_,_,rug,symbol=unpack(CF,m.c[id])
  local q=0
  if m.p[who] then local _; _,q=holding_data(m.p[who],id,PS) end
  local holders=0
  for i=1,#m.p do local _,n=holding_data(m.p[i],id,PS); if n>0 then holders=holders+1 end end
  return symbol:gsub('%z',''),supply,creator,cf+hf,meme,holders,rug,q
end
function M.cost(q,n)
  if math.type(q)~='integer' or math.type(n)~='integer' or q<0 or n<1 or q+n>10000 then return nil end
  return n*100 + (5*n*(2*q+n-1))//2
end
-- Sequential mutations: duplicates replay their original result, never mutate twice.
function M.request(m,id,seq,op,a,b,now)
  local p=player(m,id); if not p then return 2,0 end
  local hash=C.crc(string.char(op)..pack('<I2',a or 0)..(b or ''))
  if seq==p[9] then
    if hash==p[11] then return p[7],p[8],p[12] end
    return 3,0
  end
  if seq~=p[3] then return 3,0 end
  if seq>=65535 then return 16,0 end
  local code,which=0,0
  local c=coin(m,a)
  if op==1 then -- create, a is preset icon, b is symbol
    if not b or #b<2 or #b>5 or not b:match('^[A-Z][A-Z0-9]+$') then code=12
    elseif #m.c>=M.MAX_COINS then code=1
    elseif m.epoch<p[6] then code=10
    elseif p[2]<2000 then code=4
    else
      for i=1,#m.c do
        local other=coin(m,i)
        if other[1]==id and other[9]==0 then code=11 end
        if other[10]:gsub('%z','')==b then code=12 end
      end
      if code==0 then
        which=#m.c+1; p[2]=p[2]-2000
        putc(m,which,{id,0,0,0,0,0,0,m.epoch,0,b})
      end
    end
  elseif op==2 or op==3 then -- buy or sell, amount is a 16-bit LE payload
    local n=b and #b==2 and unpack('<I2',b) or 0
    if not c or (op==2 and c[9]>0) then code=8
    elseif n<1 or n>10000 then code=5
    else
      local slot,q,e,start=holding(p,a)
      local gross=M.cost(op==2 and c[2] or c[2]-n,n)
      if op==3 and q<n then code=7
      elseif not gross then code=5
      else
        local fee=c[9]>0 and 0 or (gross+49)//50
        if op==2 and not slot then code=6
        elseif op==2 and p[2]<gross+fee then code=4
        elseif op==3 and q<n then code=7
        elseif op==3 and c[3]<gross then code=8
        elseif op==3 and p[2]+gross-fee>1000000000 then code=5
        else
          local sign=op==2 and 1 or -1
          p[2]=p[2]-sign*gross-fee; c[2]=c[2]+sign*n; c[3]=c[3]+sign*gross
          if c[9]==0 then c[4]=c[4]+fee//2; c[5]=c[5]+fee-fee//2
          else p[2]=p[2]+fee end -- exits after rug are fee-free
          if op==2 then
            if q==0 then start=m.epoch; e=0 end
            if c[7] & (1<<(id-1))==0 then c[7]=c[7] | (1<<(id-1)); c[6]=math.min(1000,c[6]+10) end
          else e=math.min(e,q-n); start=m.epoch end
          puth(p,slot,a,q+sign*n,e,start); putc(m,a,c); which=a
        end
      end
    end
  elseif op==4 then -- creator withdrawal or full rug
    local percent=b and b:byte(1) or 0
    if not c or c[9]>0 then code=8
    elseif c[1]~=id then code=9
    elseif percent~=10 and percent~=25 and percent~=50 and percent~=100 then code=5
    elseif p[2]+(percent==100 and c[4]+c[5] or c[4]*percent//100)>1000000000 then code=5
    else
      local amount=c[4]*percent//100
      if percent==100 then
        amount=c[4]+c[5]; c[5]=0; c[9]=1; c[6]=0
        p[4]=math.max(0,p[4]-35); p[5]=p[5]+1; p[6]=m.epoch+3
      end
      c[4]=c[4]-(percent==100 and c[4] or amount); p[2]=p[2]+amount
      putc(m,a,c); which=a
    end
  elseif op==5 then -- host issues unpredictable one-use reaction ticket
    if p[13]>0 and now-p[13]<20000 then code=13
    elseif p[10]>=6000 then code=15
    else p[12]=math.max(1,C.seed(m.id*65536+now+id+seq)); p[13]=math.max(1,now); p[14]=now end
  elseif op==6 then -- 8 response keys plus 8 times in 20 ms units
    if p[12]==0 or not b or #b~=16 or now-p[14]>45000 or now-p[14]<4000 then code=14
    else
      local score,total=0,0
      for i=1,8 do
        local key,delay=C.command(p[12],i); local press,dt=b:byte(i),b:byte(i+8)*20
        if press>3 or dt<100 or dt>2400 then code=14 end
        if press==key and dt>=100 and dt<=1800 then score=score+250 end
        total=total+delay+dt
      end
      if total>now-p[14]+1000 then code=14 end
      if code==0 then local reward=math.min(score,6000-p[10],1000000000-p[2]); p[2]=p[2]+reward; p[10]=p[10]+reward end
    end
    p[12]=0
  else code=5 end
  p[3]=seq+1; p[7]=code; p[8]=which; p[9]=seq; p[11]=hash
  putp(m,id,p)
  return code,which,p[12]
end
function M.epoch(m)
  if m.epoch>=65000 then return false end
  m.epoch=m.epoch+1
  for id=1,#m.c do
    local c=coin(m,id); local weight=0
    for i=1,#m.p do
      local _,_,e,start=holding_data(m.p[i],id,PS)
      weight=weight+e*math.min(125,100+math.max(0,m.epoch-start-2)*10)
    end
    local pool=c[9]==0 and c[5]//2 or 0
    for i=1,#m.p do
      local slot,q,e,start=holding_data(m.p[i],id,PS)
      if q>0 then
        local p=player(m,i)
        local reward=weight>0 and pool*e*math.min(125,100+math.max(0,m.epoch-start-2)*10)//weight or 0
        reward=math.min(reward,1000000000-p[2]); p[2]=p[2]+reward; c[5]=c[5]-reward
        puth(p,slot,id,q,q,start); putp(m,i,p)
      end
    end
    c[6]=c[6]*95//100; putc(m,id,c)
    if c[9]==0 and c[2]>0 and (m.epoch-c[8])%3==0 then
      local owner=player(m,c[1]); owner[4]=math.min(100,owner[4]+1); putp(m,c[1],owner)
    end
  end
  for i=1,#m.p do local p=player(m,i); p[10]=0; p[12]=0; p[13]=0; putp(m,i,p) end
  m.dirty=true; return true
end
function M.encode(m)
  return C.wrap(pack('<c4I4I2I2I1I1','BMS1',m.gen+1,m.id,m.epoch,#m.p,#m.c)..table.concat(m.p)..table.concat(m.c))
end
function M.decode(s)
  if type(s)~='string' or #s<18 or #s>4096 or s:sub(1,4)~='BMS1' or not C.valid(s) then return nil end
  local _,gen,id,epoch,np,nc,pos=unpack('<c4I4I2I2I1I1',s)
  if np>12 or nc>16 or epoch<1 or epoch>65000 or #s~=18+np*(PS+28)+nc*CS then return nil end
  local m=M.new(id); m.gen=gen; m.epoch=epoch
  for i=1,np do
    m.p[i]=s:sub(pos,pos+PS+27); pos=pos+PS+28
    local p=player(m,i)
    if p[2]>1000000000 or p[3]==0 then return nil end
    for j=1,4 do local cid,q,e=unpack(HF,p[15],(j-1)*7+1); if cid>nc or q>10000 or e>q then return nil end end
    p[12]=0; p[13]=0; p[14]=0; putp(m,i,p)
  end
  for i=1,nc do
    m.c[i]=s:sub(pos,pos+CS-1); pos=pos+CS
    local c=coin(m,i)
    if c[1]<1 or c[1]>np or c[2]>10000 or c[9]>1 or c[3]~=(c[2]>0 and M.cost(0,c[2]) or 0) then return nil end
    local supply=0
    for j=1,np do local _,q=holding(player(m,j),i); supply=supply+q end
    if supply~=c[2] then return nil end
  end
  m.dirty=false; return m
end
return M
