-- Capture the console-visible result of a fixed Doom stream after startup.
local output = assert(os.getenv("FCPICO_RESULTS"))
local limit = tonumber(os.getenv("FCPICO_FRAMES")) or 180
local frames = 0
local nmi_rti = os.getenv("FCPICO_NMI_RTI")
local nmi_count = 0
local nmi_max_scanline = -1
local nmi_min_scanline = 1000

if nmi_rti then
    emu.addMemoryCallback(function()
        if frames >= limit - 30 then
            local scanline = emu.getState()["ppu.scanline"]
            nmi_count = nmi_count + 1
            nmi_max_scanline = math.max(nmi_max_scanline, scanline)
            nmi_min_scanline = math.min(nmi_min_scanline, scanline)
        end
    end, emu.callbackType.exec, tonumber(nmi_rti, 16))
end

local function save_ppu_bytes(name, first, count)
    local file = assert(io.open(output .. "/" .. name, "wb"))
    for address = first, first + count - 1 do
        file:write(string.char(emu.read(address, emu.memType.nesPpuDebug)))
    end
    file:close()
end

local function save_cpu_bytes(name, first, count)
    local file = assert(io.open(output .. "/" .. name, "wb"))
    for address = first, first + count - 1 do
        file:write(string.char(emu.read(address, emu.memType.nesDebug)))
    end
    file:close()
end

emu.addEventCallback(function()
    frames = frames + 1
    if frames == limit then
        local png = assert(io.open(output .. "/final.png", "wb"))
        png:write(emu.takeScreenshot())
        png:close()
        local prg = assert(io.open(output .. "/prg.bin", "wb"))
        for address = 0, 0x7fff do
            prg:write(string.char(emu.read(address, emu.memType.nesPrgRom)))
        end
        prg:close()
        save_ppu_bytes("palette.bin", 0x3f00, 16)
        save_ppu_bytes("attributes.bin", 0x23c0, 64)
        if nmi_rti then
            save_cpu_bytes("mailbox.bin", 0x20, 64)
            local file = assert(io.open(output .. "/mailbox.bin", "ab"))
            for address = 0xc0, 0xff do
                file:write(string.char(emu.read(address, emu.memType.nesDebug)))
            end
            file:close()
        end
        local result = assert(io.open(output .. "/lua_result.json", "w"))
        result:write(string.format('{"frames":%d,"nmi_count":%d,"nmi_min_scanline":%d,"nmi_max_scanline":%d}\n',
                                   frames, nmi_count, nmi_min_scanline, nmi_max_scanline))
        result:close()
        emu.stop(0)
    end
end, emu.eventType.endFrame)
