local T=require('copy')
-- BADGE MARKET: fake SOL, real social consequences.
-- HOME saves/exits. START returns to hub (except hold-to-rug). AUX1 dims LEDs.
local U=require('ui')
local S=require('session')
local B,K=badge.input.BUTTON,badge.input.KIND
local scene,select,coin,amount='boot',1,1,1
local symbol,cursor=T(1),1
local alphabet=T(2)
local G,D,dirty,now,ledAt,level=nil,nil,true,0,0,110
local rugAt,alertUntil,requestGame=0,0,false
local lastPaint,maxTick,diagAt=0,0,0
local function go(name)
  scene=name; select=1; dirty=true; rugAt=0
  if name~='reaction' and name~='maze' and type(G)=='table' then G.kind='' end
  badge.sys.gc_step()
end
local function startgame(kind)
  if not G then
    badge.sys.gc_step()
    local s=badge.sys.stats()
    if s.lua_used+11264>s.lua_limit or s.free_heap<49152 then
      S.status='Memory busy. Play on a client.'; S.changed=true; return
    end
    G=require('games')
  end
  if kind=='reaction' then
    if S.act(5,0,'') then requestGame=true; S.status=T(3) end
  else G.start('maze',1,now); go('maze') end
end
local function draw()
  local d=(scene=='coin' or scene=='admin') and S.detail(coin) or nil
  if scene=='boot' then
    U.page(T(4),T(5),T(6),T(7),T(8))
    U.row(1,T(9),select==1); U.row(2,T(10),select==2)
    U.row(3,T(11),select==3); U.row(5,S.status)
  elseif scene=='home' then
    U.page(T(4),S.mode:upper(),U.sol(S.balance)..T(12),T(13)..S.epoch..T(14),T(15))
    U.row(1,T(16)); U.row(2,T(17))
    U.row(3,T(18)); U.row(4,T(19))
    U.row(5,S.status)
  elseif scene=='market' or scene=='portfolio' then
    U.page(scene=='market' and T(20) or T(21),'E'..S.epoch,
      U.sol(S.balance)..T(12),S.count..T(22),T(23))
    if S.count==0 then U.row(2,T(24)) end
    local first=math.max(1,coin-2)
    for i=1,4 do
      local id=first+i-1
      if id<=S.count then
        local c=S.detail(id)
        U.row(i,c and ('$'..c[1]..T(25)..U.sol(100+5*c[2])..(scene=='portfolio' and (T(26)..c[8]) or (c[7]>0 and T(27) or T(12)))) or T(28),id==coin)
        if not c then S.fetch(id) end
      end
    end
    U.row(5,S.status)
  elseif scene=='coin' then
    if not d then U.page(T(29),T(30),T(31),T(32),T(33)); S.fetch(coin)
    else
      U.page('$'..d[1],d[7]>0 and T(34) or T(35),U.sol(100+5*d[2])..T(12),
        T(36)..d[8]..T(37)..d[6]..T(38)..d[5],T(39))
      U.row(1,T(40)..amount..T(41))
      U.row(2,T(42)..d[2]..T(43)..U.sol(d[4]))
      U.row(3,d[3]==S.id and T(44) or T(45))
      U.row(4,T(46)); U.row(5,S.status)
    end
  elseif scene=='create' then
    U.page(T(47),T(48),'$'..symbol,T(49)..cursor,T(50))
    U.row(1,T(51))
    U.row(2,T(52))
    U.row(3,T(53))
    U.row(4,T(54)); U.row(5,S.status)
  elseif scene=='games' then
    U.page(T(55),T(56),T(57),T(58),T(59))
    U.row(1,T(60),select==1)
    U.row(2,T(61),select==2)
    U.row(3,T(62),select==3); U.row(5,S.status)
  elseif scene=='reaction' or scene=='maze' then G.render(U)
  elseif scene=='nearby' then
    U.page(S.mode=='offline' and T(63) or T(64),T(65),T(66),
      T(67),S.mode=='offline' and T(68) or T(69))
    local live=0
    local list=S.mode=='offline' and S.peers or S.players
    for i=1,#list do
      if now-list[i][S.mode=='offline' and 4 or 3]<8000 then
        live=live+1
        if live<=4 then U.row(live,(S.mode=='offline' and T(70) or T(71))..list[i][2],i==select) end
      end
    end
    if live==0 then U.row(2,T(72)) end
    U.row(5,S.status)
  elseif scene=='duel' then D.render(U)
  elseif scene=='profile' then
    U.page(T(73),S.save,T(74)..S.rep..T(75),T(76)..S.rugs,T(77))
    U.row(1,S.rugs==0 and T(78) or T(79))
    U.row(2,T(70)..S.market..T(80)..S.epoch)
    U.row(3,T(81)..level..T(82))
    local stats=badge.sys.stats()
    U.row(4,T(83)..stats.lua_used..T(84)..stats.lua_limit)
    U.row(5,T(85)..stats.free_heap..T(86)..maxTick..'ms')
  elseif scene=='admin' then
    U.page(T(87),T(88),d and ('$'..d[1]) or T(89),
      T(90),T(91))
    U.row(1,T(92),select==1); U.row(2,T(93),select==2)
    U.row(3,T(94),select==3); U.row(4,T(95),select==4); U.row(5,S.status)
  elseif scene=='rug' then
    U.page(T(96),T(97),T(98),T(99),T(100))
    U.text(3,T(98),U.red)
    U.row(1,T(101))
    U.row(2,T(102))
    U.row(4,rugAt>0 and ('['..string.rep('#',math.min(12,(now-rugAt)//250))..']') or T(103))
  elseif scene=='alert' then
    local c=S.detail(S.rug or coin)
    U.page(T(104),T(105),c and ('$'..c[1]..T(106)) or T(107),
      T(108),T(109))
    U.text(3,c and ('$'..c[1]..T(106)) or T(107),U.red)
    U.row(2,T(110)); U.row(4,T(111))
  end
  U.flush(); dirty=false; S.changed=false; lastPaint=now
end
local function leds()
  if now<ledAt then return end
  ledAt=now+100; badge.led.clear()
  if level>0 then
    if scene=='alert' or scene=='rug' then
      local v=(now//400)%2==0 and level or level//3
      badge.led.set_all(v,0,0)
    elseif scene=='reaction' and G.phase==2 then
      if G.command==0 then badge.led.set_all(0,level,0)
      elseif G.command==1 then badge.led.set_all(level,0,0)
      else badge.led.set_all(0,0,level) end
    else badge.led.set((now//250)%6+1,level//2,level,level//5) end
  end
  badge.led.show()
end
function on_enter(root)
  now=badge.sys.ms(); S.tick(now); U.init(root); draw()
  U.init=nil; on_enter=nil -- widgets are reused for the entire VM lifetime
  badge.sys.log(T(112)..badge.sys.version()..T(113))
end
function on_tick()
  local begin=badge.sys.ms(); now=begin; S.tick(now)
  if S.mode~='offline' and S.mode~='solo' and D==nil then
    local s=badge.sys.stats()
    if s.lua_used+11264>s.lua_limit or s.free_heap<49152 then D=false
    else D=require('duel'); D.init() end
  end
  if D then
    D.tick(now)
    if S.invite then S.invite=nil; go('duel') end
    if scene=='duel' then dirty=dirty or D.dirty end
  end
  if requestGame and S.ticket>0 then
    requestGame=false; G.start('reaction',S.ticket,now); go('reaction')
  elseif requestGame and not S.busy() then requestGame=false end
  if scene=='reaction' or scene=='maze' then G.tick(now); dirty=dirty or G.dirty end
  if S.rug and scene~='alert' then go('alert'); alertUntil=now+8000 end
  if scene=='alert' and now>=alertUntil then S.rug=nil; go('home') end
  if scene=='rug' and rugAt>0 then
    if not badge.input.is_down(B.START) then rugAt=0
    elseif now-rugAt>=3000 then rugAt=0; S.act(4,coin,string.char(100)); go('home') end
    dirty=true
  end
  if (dirty or S.changed) and now-lastPaint>=50 then draw() end
  leds(); maxTick=math.max(maxTick,badge.sys.ms()-begin)
  if now>=diagAt then
    diagAt=now+30000; local s=badge.sys.stats()
    badge.sys.log('lua='..s.lua_used..T(114)..s.lua_peak..T(115)..s.free_heap..T(116)..s.widgets..T(117)..maxTick..T(118)..badge.radio.dropped())
  end
end
function on_button(button,kind)
  now=badge.sys.ms()
  if kind~=K.PRESSED then if button==B.START then rugAt=0 end; return end
  if button==B.AUX1 then level=level==0 and 110 or 0; dirty=true; return end
  if scene=='rug' then
    if button==B.START then rugAt=now elseif button==B.B then go('admin') end
  elseif button==B.START then
    if S.busy() then S.retry() else go(S.mode=='offline' and 'boot' or 'home') end
  elseif scene=='boot' then
    if button==B.UP then select=(select+1)%3+1 elseif button==B.DOWN then select=select%3+1
    elseif button==B.A then
      if select==3 then S.connect(); go('nearby')
      elseif S.start(select==1 and 'solo' or 'host') then go('home') end
    end
  elseif scene=='home' then
    if button==B.A then go('market') elseif button==B.B then go('games')
    elseif button==B.UP then go('portfolio') elseif button==B.DOWN then go('create')
    elseif button==B.LEFT then S.connect(); go('nearby') elseif button==B.RIGHT then go('profile') end
  elseif scene=='market' or scene=='portfolio' then
    if button==B.UP then coin=math.max(1,coin-1) elseif button==B.DOWN then coin=math.max(1,math.min(S.count,coin+1))
    elseif button==B.A and S.count>0 then go('coin') elseif button==B.B then go('home') end
  elseif scene=='coin' then
    if button==B.A then S.act(2,coin,string.pack('<I2',amount))
    elseif button==B.B then S.act(3,coin,string.pack('<I2',amount))
    elseif button==B.LEFT then amount=math.max(1,amount-1) elseif button==B.RIGHT then amount=math.min(100,amount+1)
    elseif button==B.UP then go('market') elseif button==B.DOWN then local d=S.detail(coin); if d and d[3]==S.id then go('admin') end end
  elseif scene=='create' then
    if button==B.LEFT then cursor=(cursor+3)%5+1 elseif button==B.RIGHT then cursor=cursor%5+1
    elseif button==B.UP or button==B.DOWN then
      local i=alphabet:find(symbol:sub(cursor,cursor),1,true) or 1
      i=(i+(button==B.UP and 0 or -2))%#alphabet+1
      if cursor==1 and i>26 then i=button==B.UP and 1 or 26 end
      symbol=symbol:sub(1,cursor-1)..alphabet:sub(i,i)..symbol:sub(cursor+1)
    elseif button==B.A then S.act(1,0,symbol)
    elseif button==B.B then go('home') end
  elseif scene=='games' then
    if button==B.UP then select=(select+1)%3+1 elseif button==B.DOWN then select=select%3+1
    elseif button==B.A then
      if select==3 then S.connect(); go('nearby') else startgame(select==1 and 'reaction' or 'maze') end
    elseif button==B.B then go('home') end
  elseif scene=='reaction' then
    if G.phase==3 and button==B.A then S.act(6,0,G.proof); go('home')
    elseif button==B.A then G.press(0,now) elseif button==B.B then G.press(1,now) elseif button==B.UP then G.press(2,now) end
  elseif scene=='maze' then if button==B.B then go('games') end
  elseif scene=='nearby' then
    if button==B.UP then select=math.max(1,select-1) elseif button==B.DOWN then select=math.min(S.mode=='offline' and #S.peers or #S.players,select+1)
    elseif button==B.A and S.mode=='offline' then S.join(select); go('home')
    elseif button==B.A and D then if D.challenge(select,now) then go('duel') end
    elseif button==B.A then S.status='Duel needs room. Try a client.'
    elseif button==B.B then go(S.mode=='offline' and 'boot' or 'home') end
  elseif scene=='duel' then if not D.press(button,now) then go('home') end
  elseif scene=='profile' then
    if button==B.A then S.checkpoint() elseif button==B.LEFT then level=math.max(0,level-22)
    elseif button==B.B then go('home') end
  elseif scene=='admin' then
    if button==B.UP then select=(select+2)%4+1 elseif button==B.DOWN then select=select%4+1
    elseif button==B.A then if select==4 then go('rug') else S.act(4,coin,string.char(({10,25,50})[select])) end
    elseif button==B.B then go('coin') end
  elseif scene=='alert' and button==B.A then S.rug=nil; go('home') end
  dirty=true
end
function on_exit()
  S.stop(); badge.led.clear(); badge.led.show()
end
