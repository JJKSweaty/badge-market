-- Fixed-width little-endian wire/storage records. No JSON or executable saves.
local M = {}
function M.crc(s)
  local c = 0xffffffff
  for i = 1, #s do
    c = c ~ s:byte(i)
    for _ = 1, 8 do c = (c >> 1) ~ (0xedb88320 & -(c & 1)) end
  end
  return (~c) & 0xffffffff
end
function M.wrap(s) return s .. string.pack('<I4', M.crc(s)) end
function M.valid(s)
  return type(s) == 'string' and #s >= 4 and
    M.crc(s:sub(1, -5)) == string.unpack('<I4', s, #s - 3)
end
-- BM1 + type + 16-bit market + 16-bit sequence + payload + CRC32 <= 44.
function M.frame(kind, market, seq, data)
  local s = string.pack('<c3I1I2I2', 'BM1', kind, market, seq) .. (data or '')
  if #s > 40 then return nil end
  return M.wrap(s)
end
function M.parse(s)
  if #s < 12 or #s > 44 or s:sub(1, 3) ~= 'BM1' or not M.valid(s) then return end
  local _, kind, market, seq = string.unpack('<c3I1I2I2', s)
  return kind, market, seq, s:sub(9, -5)
end
function M.mac(s)
  if type(s) ~= 'string' or not s:match('^%x%x:%x%x:%x%x:%x%x:%x%x:%x%x$') then return nil end
  return (s:gsub(':',''):gsub('%x%x', function(x) return string.char(tonumber(x, 16)) end))
end
function M.seed(x) return (1664525 * x + 1013904223) & 0xffffffff end
-- Advance one seeded command at a time. 0=BUY/A, 1=SELL/B, 2=HOLD/UP.
function M.command(seed, round)
  for _ = 1, round do seed = M.seed(seed) end
  return (seed >> 16) % 3, 400 + (seed % 5) * 100
end
return M
