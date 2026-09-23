-- Capture the console-visible result of a fixed Doom stream after startup.
local output = assert(os.getenv("FCPICO_RESULTS"))
local limit = tonumber(os.getenv("FCPICO_FRAMES")) or 180
local frames = 0

local function save_ppu_bytes(name, first, count)
    local file = assert(io.open(output .. "/" .. name, "wb"))
    for address = first, first + count - 1 do
        file:write(string.char(emu.read(address, emu.memType.nesPpuDebug)))
    end
    file:close()
end

emu.addEventCallback(function()
    frames = frames + 1
    if frames == limit then
        local png = assert(io.open(output .. "/final.png", "wb"))
        png:write(emu.takeScreenshot())
        png:close()
        save_ppu_bytes("palette.bin", 0x3f00, 16)
        save_ppu_bytes("attributes.bin", 0x23c0, 64)
        local result = assert(io.open(output .. "/lua_result.json", "w"))
        result:write(string.format('{"frames":%d}\n', frames))
        result:close()
        emu.stop(0)
    end
end, emu.eventType.endFrame)
