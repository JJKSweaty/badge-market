local T=require('copy')
-- Reaction-time duel: local elapsed times avoid depending on synchronized clocks.
-- Seed/start/result/ACK only. Modified clients can lie: this awards bragging rights.
local S=require('session')
local D={phase='idle',dirty=true}
local peer,nonce,seed,at,limit,packet,nextSend,tries,result,theirs
local pack,unpack=string.pack,string.unpack
local function queue(kind,payload,now)
  packet={kind,payload}; nextSend=now; tries=0
end
local function receive(from,kind,seq,data,now)
  if kind==20 and #data==4 and (D.phase=='idle' or D.phase=='done') then
    local found=false
    for i=1,#S.players do if S.players[i][1]==from and now-S.players[i][3]<8000 then found=true end end
    if not found then return end
    peer=from; nonce=seq; seed=unpack('<I4',data); limit=now+12000; D.phase='invite'; D.dirty=true
    S.invite=true; return
  end
  if from~=peer or seq~=nonce then return end
  if kind==21 and #data==0 and D.phase=='offer' then
    D.phase='wait'; at=now+1500+seed%2000; limit=at+10000
    queue(22,'',now)
  elseif kind==22 and #data==0 then
    if D.phase=='accept' then D.phase='wait'; at=now+1500+seed%2000; limit=at+10000; packet=nil end
    S.send(23,peer,nonce,'')
  elseif kind==23 and #data==0 then packet=nil
  elseif kind==24 and #data==2 then
    theirs=unpack('<I2',data); S.send(25,peer,nonce,'')
    if result then D.phase='done' end
  elseif kind==25 and #data==0 then packet=nil
  elseif kind==26 then D.phase='cancelled'; packet=nil end
  D.dirty=true
end
function D.init() S.listener=receive end
function D.challenge(index,now)
  local p=S.players[index]
  if not p or now-p[3]>8000 then return false end
  peer=p[1]; nonce=badge.sys.random(65534)+1; seed=badge.sys.random()
  result=nil; theirs=nil; D.phase='offer'; D.dirty=true; limit=now+12000
  queue(20,pack('<I4',seed),now); return true
end
function D.press(button,now)
  local B=badge.input.BUTTON
  if button==B.B then
    if peer then S.send(26,peer,nonce,'') end
    D.phase='idle'; packet=nil; return false
  end
  if button~=B.A then return true end
  if D.phase=='invite' then
    result=nil; theirs=nil; D.phase='accept'; queue(21,'',now); S.invite=nil
  elseif D.phase=='wait' or D.phase=='go' then
    result=D.phase=='wait' and 65535 or math.min(9999,now-at)
    if result<100 then result=65535 end
    D.phase=theirs and 'done' or 'result'; queue(24,pack('<I2',result),now)
  elseif D.phase=='done' or D.phase=='cancelled' then D.phase='idle'; return false end
  D.dirty=true; return true
end
function D.tick(now)
  if D.phase=='idle' then return end
  if packet and now>=nextSend and tries<8 then
    S.send(packet[1],peer,nonce,packet[2]); nextSend=now+700; tries=tries+1
  end
  if D.phase=='wait' and now>=at then D.phase='go'; D.dirty=true end
  if D.phase=='go' and now-at>=5000 then D.press(badge.input.BUTTON.A,now) end
  if now>=limit and D.phase~='done' then D.phase='cancelled'; packet=nil; D.dirty=true end
end
function D.render(U)
  local text=D.phase=='go' and T(144) or D.phase=='wait' and T(145) or
    D.phase=='invite' and T(146) or D.phase=='done' and
    (result==theirs and T(147) or result<theirs and T(148) or T(149)) or
    D.phase=='cancelled' and T(150) or T(151)
  U.page(T(152),T(153),text,T(154),T(155))
  if result then U.row(1,result==65535 and T(156) or (T(157)..result..T(158))) end
  if theirs then U.row(2,theirs==65535 and T(159) or (T(160)..theirs..T(158))) end
  U.row(4,T(161)); D.dirty=false
end
return D
