local frames = 0
emu.addEventCallback(function()
  frames = frames + 1
  if frames == 30 then
    local path = assert(os.getenv("HUD_VARIANT_CAPTURE"))
    local file = assert(io.open(path, "wb"))
    file:write(emu.takeScreenshot())
    file:close()
    emu.stop(0)
  end
end, emu.eventType.endFrame)
