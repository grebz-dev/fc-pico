-- SPDX-License-Identifier: BSD-3-Clause
-- Runs FCPICO_FRAMES frames. With FCPICO_DEBUG_PEEKS=1 it also reads every pattern
-- table and nametable address through the debugger each frame; those reads must not
-- reach the cartridge.
local output = assert(os.getenv("FCPICO_RESULTS"))
local limit = tonumber(os.getenv("FCPICO_FRAMES")) or 300
local peeks = os.getenv("FCPICO_DEBUG_PEEKS") == "1"
local frames = 0

emu.addEventCallback(function()
    frames = frames + 1
    if peeks then
        for address = 0, 0x2fff do emu.read(address, emu.memType.nesPpuDebug) end
    end
    if frames == limit then
        local png = assert(io.open(output .. "/final.png", "wb"))
        png:write(emu.takeScreenshot())
        png:close()
        local result = assert(io.open(output .. "/lua_result.json", "w"))
        result:write(string.format('{"frames":%d}\n', frames))
        result:close()
        emu.stop(0)
    end
end, emu.eventType.endFrame)
