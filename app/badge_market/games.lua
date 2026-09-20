local T=require('copy')
-- One active minigame. No table creation in physics ticks.
local C=require('codec')
local G={kind='',phase=0,score=0,round=0,dirty=true}
local seed,at,deadline,keys,times=0,0,0,{},{}
local x,y,vx,vy,stepAt,goal,maze=0,0,0,0,0,0,{}
function G.start(kind,s,t)
  G.kind=kind; G.score=0; G.round=0; G.dirty=true; seed=s
  G.phase=0; at=t; deadline=t+800; keys={}; times={}
  if kind=='maze' then
    x=1; y=1; vx=0; vy=0; goal=0; stepAt=t
    -- Seven rows as bit masks: a small deterministic corridor, not a pixel map.
    maze={127,65,93,81,87,65,127}; G.phase=1
  end
end
function G.tick(t)
  if G.kind=='reaction' then
    if G.phase==0 and t>=deadline then
      G.round=G.round+1
      if G.round>8 then G.phase=3; G.proof=string.char(table.unpack(keys))..string.char(table.unpack(times))
      else
        local delay; G.command,delay=C.command(seed,G.round)
        G.phase=1; deadline=t+delay
      end
      G.dirty=true
    elseif G.phase==1 and t>=deadline then G.phase=2; at=t; G.dirty=true
    elseif G.phase==2 and t-at>=1800 then
      keys[G.round]=3; times[G.round]=90; G.phase=0; deadline=t+180; G.dirty=true
    end
  elseif G.kind=='maze' and G.phase==1 and t>=stepAt then
    stepAt=t+100 -- fixed, capped: missed work is dropped rather than replayed
    local ax,ay=badge.sensor.accel()
    if ax then vx=ax>180 and 1 or ax< -180 and -1 or 0; vy=ay>180 and 1 or ay< -180 and -1 or 0
    else vx=0; vy=0 end
    if badge.input.is_down(badge.input.BUTTON.LEFT) then vx=-1 end
    if badge.input.is_down(badge.input.BUTTON.RIGHT) then vx=1 end
    if badge.input.is_down(badge.input.BUTTON.UP) then vy=-1 end
    if badge.input.is_down(badge.input.BUTTON.DOWN) then vy=1 end
    local nx,ny=x+vx,y+vy
    if nx>=1 and nx<=5 and (maze[y+1] & (1<<nx))==0 then x=nx end
    if ny>=1 and ny<=5 and (maze[ny+1] & (1<<x))==0 then y=ny end
    if x==5 and y==5 then G.phase=3; G.score=t-at end
    if vx~=0 or vy~=0 then G.dirty=true end
  end
end
function G.press(key,t)
  if G.kind~='reaction' or G.round<1 or G.round>8 or (G.phase~=1 and G.phase~=2) then return end
  local elapsed=G.phase==2 and math.min(1800,t-at) or 100
  local accepted=G.phase==2 and elapsed>=100 and key==G.command
  keys[G.round]=G.phase==1 and 3 or key; times[G.round]=math.max(5,(elapsed+19)//20)
  if accepted then G.score=G.score+1 end
  G.phase=0; deadline=t+180; G.dirty=true
end
function G.render(U)
  if G.kind=='reaction' then
    local text=G.phase==1 and T(119) or G.phase==2 and ({T(120),T(121),T(122)})[G.command+1]
      or G.phase==3 and T(123) or T(124)
    U.page(T(125),T(126),text,T(127),T(128))
    U.row(1,T(129)..math.min(8,G.round)..T(130))
    U.row(2,T(131)..G.score..T(130))
    U.row(4,G.phase==3 and T(132) or T(133))
    U.text(3,text,G.phase==2 and U.lime or U.blue)
  else
    U.page(T(134),T(135),G.phase==3 and T(136) or T(137),
      T(138),T(139))
    for row=1,5 do
      local s=''
      for col=1,5 do
        s=s..((x==col and y==row) and T(140) or (col==5 and row==5) and T(141) or
          (maze[row+1] & (1<<col))~=0 and T(142) or T(143))
      end
      U.row(row,s)
    end
  end
  G.dirty=false
end
return G
