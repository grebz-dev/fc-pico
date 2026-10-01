local output = assert(os.getenv("HUD_PLAYBACK_OUTPUT"))
local frames = 0
emu.addEventCallback(function()
  frames = frames + 1
  local number = frames == 30 and 1 or frames == 90 and 2 or frames == 150 and 3 or 0
  if number ~= 0 then
    local file = assert(io.open(string.format("%s/frame-%02d.png", output, number), "wb"))
    file:write(emu.takeScreenshot())
    file:close()
    local debug = assert(io.open(string.format("%s/ppu-%02d.txt", output, number), "w"))
    for _, address in ipairs({0x2000, 0x2001, 0x2002, 0x2003, 0x23c0, 0x3f00}) do
      debug:write(string.format("%04X %02X\n", address,
        emu.read(address, emu.memType.nesPpuDebug)))
    end
    for _, address in ipairs({0x10, 0x11, 0x12, 0x2000, 0x2001}) do
      debug:write(string.format("CPU %04X %02X\n", address,
        emu.read(address, emu.memType.nesDebug)))
    end
    for key, value in pairs(emu.getState()) do
      if string.find(key, "ppu") then
        debug:write(string.format("%s %s\n", key, tostring(value)))
      end
    end
    debug:close()
    if number == 3 then emu.stop(0) end
  end
end, emu.eventType.endFrame)
