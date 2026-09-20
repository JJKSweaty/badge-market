local T=dofile('tests/sandbox.lua')
T.new('01:02:03:04:05:06');T.new('01:02:03:04:05:07')
local function quote(s) return '"'..tostring(s):gsub('\\','\\\\'):gsub('"','\\"'):gsub('\n','\\n'):gsub('\r','')..'"' end
local function state()
 local out={}
 for index,d in ipairs(T.devices) do
  local rows={}
  for _,v in ipairs(d.widgets) do
   local st=v.styledata
   rows[#rows+1]=string.format('{"text":%s,"x":%d,"y":%d,"w":%d,"h":%d,"font":%d,"color":%d,"bg":%d}',
    quote(v.text),v.x,v.y,v.w,v.h,st.text_font or 16,st.text_color or 0xe9f1e7,st.bg_color or -1)
  end
  out[#out+1]='['..table.concat(rows,',')..']'
 end
 print('['..table.concat(out,',')..']');io.stdout:flush()
end
state()
for line in io.lines() do
 local op,a,b=line:match('^(%w+) ?(%w*) ?(%w*)')
 if op=='key' then T.press(T.devices[tonumber(a)],b)
 elseif op=='tick' then T.advance(math.min(2000,tonumber(a) or 100))
 elseif op=='hold' then local d=T.devices[tonumber(a)];d.held[9]=b=='on';d.env.on_button(9,b=='on' and 1 or 2)
 elseif op=='loss' then T.drop=tonumber(a) or 0 end
 state()
end
