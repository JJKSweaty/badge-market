-- Reusable LVGL widget pool. Text/style changes are cached; LVGL owns DMA/dirty areas.
local U={ink=0xe9f1e7,muted=0x8c9c9b,lime=0xc7f45c,red=0xff657a,blue=0x6db9ff}
local labels,cache,wanted,colors,painted={},{},{},{},{}
local function label(parent,x,y,w,font,color)
  local v=badge.ui.label(parent,'')
  v:set_pos(x,y); v:set_size(w,28)
  v:style({text_font=font,text_color=color,pad_all=0})
  return v
end
function U.init(root)
  local bg=badge.ui.box(root,320,240); bg:set_pos(0,0)
  bg:style({bg_color=0x101918,radius=0,border_width=0,pad_all=0})
  local rule=badge.ui.box(bg,292,2); rule:set_pos(14,37)
  rule:style({bg_color=U.lime,border_width=0,radius=0,pad_all=0})
  labels[1]=label(bg,14,10,200,20,U.ink)
  labels[2]=label(bg,216,14,96,14,U.lime)
  labels[3]=label(bg,14,47,292,24,U.lime)
  labels[4]=label(bg,14,77,292,14,U.muted)
  for i=1,5 do labels[4+i]=label(bg,14,96+(i-1)*22,292,16,U.ink) end
  labels[10]=label(bg,14,215,292,14,U.muted)
end
function U.text(i,s,color)
  wanted[i]=tostring(s or '')
  if color then colors[i]=color end
end
function U.flush()
  for i=1,10 do
    if wanted[i]~=cache[i] then labels[i]:set_text(wanted[i] or ''); cache[i]=wanted[i] end
    if colors[i] and colors[i]~=painted[i] then labels[i]:set_color(colors[i]); painted[i]=colors[i] end
  end
end
function U.page(title,tag,hero,sub,footer)
  U.text(1,title); U.text(2,tag); U.text(3,hero,U.lime); U.text(4,sub)
  for i=5,9 do U.text(i,'',U.ink) end
  U.text(10,footer)
end
function U.row(i,s,selected) U.text(4+i,(selected and '> ' or '  ')..s,selected and U.lime or U.ink) end
function U.sol(n) return string.format('%d.%03d',n//1000,n%1000) end
return U
