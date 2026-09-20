-- Documented API mock. This checks logic/contracts, not real LVGL timing or heap.
local M={clock=1000,devices={},queue={},drop=0,duplicate=false}
local BUTTON={A=1,B=2,HOME=3,DOWN=4,LEFT=5,RIGHT=6,UP=7,AUX1=8,START=9}
function M.new(mac,saves)
  local d={mac=mac,files=saves or {},held={},widgets={},logs={},radio=false,exited=false}
  local env={assert=assert,error=error,ipairs=ipairs,pairs=pairs,next=next,select=select,
    tonumber=tonumber,tostring=tostring,type=type,math=math,string=string,table=table,utf8=utf8}
  local function widget(parent,text,w,h)
    local v={parent=parent,text=text or '',w=w or 0,h=h or 28,x=0,y=0,styledata={}}
    function v:set_pos(x,y) assert(math.type(x)=='integer' and math.type(y)=='integer'); self.x=x;self.y=y end
    function v:set_size(a,b) self.w=a;self.h=b end
    function v:style(t) for k,val in pairs(t) do self.styledata[k]=val end end
    function v:set_text(t) assert(type(t)=='string' and #t<=1024);self.text=t end
    function v:set_color(c) self.styledata.text_color=c end
    d.widgets[#d.widgets+1]=v; assert(#d.widgets<=512); return v
  end
  local badge={ui={label=function(p,t) return widget(p,t) end,box=function(p,w,h) return widget(p,'',w,h) end},
    input={BUTTON=BUTTON,KIND={PRESSED=1,RELEASED=2},is_down=function(b) return d.held[b] or false end},
    led={clear=function() end,show=function() end,set=function(i,r,g,b) assert(i>=1 and i<=6 and r<=255 and g<=255 and b<=255) end,set_all=function() end},
    sensor={accel=function() return nil,'unavailable' end},
    sys={ms=function() return M.clock end,version=function() return 'mock' end,
      random=function(n) local v=math.random(0,0x7fffffff);return n and v%n or v end,
      gc_step=function() collectgarbage('step') end,log=function(s) d.logs[#d.logs+1]=s end,
      stats=function() return {lua_used=0,lua_peak=0,lua_limit=98304,widgets=#d.widgets,free_heap=999999} end},
    fs={read=function(p) return d.files[p] end,write=function(p,s) if d.failwrite then return false end; d.files[p]=s;return true end},
    radio={enable=function() d.radio=true;return true end,disable=function() d.radio=false end,
      mac=function() return mac end,dropped=function() return 0 end,on_recv=function(f) d.recv=f end,
      send=function(s)
        assert(#s<=44,'44-byte radio cap')
        M.queue[#M.queue+1]={d,s}; return true
      end},app={exit=function() d.exited=true end}}
  env.badge=badge
  local cache={}
  env.require=function(name)
    if cache[name] then return cache[name] end
    assert(name:match('^[%w_]+$'))
    local fn=assert(loadfile('app/badge_market/'..name..'.lua','t',env))
    cache[name]=fn(); return cache[name]
  end
  assert(loadfile('app/badge_market/main.lua','t',env))()
  env.on_enter({}); d.env=env;d.modules=cache; M.devices[#M.devices+1]=d
  return d
end
function M.press(d,b)
  b=BUTTON[b];d.held[b]=true;d.env.on_button(b,1)
  d.held[b]=false;d.env.on_button(b,2)
end
function M.advance(ms)
  for _=1,ms//20 do
    M.clock=M.clock+20
    local packets=M.queue; M.queue={}
    for _,p in ipairs(packets) do
      if M.drop>0 then M.drop=M.drop-1
      else for _,d in ipairs(M.devices) do
        if d~=p[1] and d.radio and d.recv then
          d.recv(p[1].mac,-45,p[2]);if M.duplicate then d.recv(p[1].mac,-45,p[2]) end
        end
      end
      end
    end
    for _,d in ipairs(M.devices) do d.env.on_tick() end
  end
end
function M.text(d)
  local s={};for _,v in ipairs(d.widgets) do s[#s+1]=v.text end;return table.concat(s,'\n')
end
return M
