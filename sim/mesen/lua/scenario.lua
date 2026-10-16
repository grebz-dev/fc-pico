-- SPDX-License-Identifier: BSD-3-Clause
-- Generic headless scenario driver for sim/mesen/scenario.py.
--
--   FCPICO_RESULTS          output directory (set by the runner)
--   FCPICO_FRAMES           frames to run
--   FCPICO_STARTUP_FRAMES   frames before the mailbox must be valid on every frame
--   FCPICO_DEBUG_PEEKS=1    read all pattern-table/nametable addresses via the debugger
--   FCPICO_INPUT            input script: "first last button[,button...]" per line
--   FCPICO_DUMP_PRG=1       write the console's 32 KiB PRG (after any reflash) to prg.bin
local output = assert(os.getenv("FCPICO_RESULTS"))
local limit = tonumber(os.getenv("FCPICO_FRAMES")) or 300
local startup = tonumber(os.getenv("FCPICO_STARTUP_FRAMES")) or 150
local peeks = os.getenv("FCPICO_DEBUG_PEEKS") == "1"
local dump_prg = os.getenv("FCPICO_DUMP_PRG") == "1"
local frames = 0
local valid = 0

local schedule = {}
local input_path = os.getenv("FCPICO_INPUT")
if input_path and input_path ~= "" then
    for line in io.lines(input_path) do
        local first, last, buttons = line:match("^%s*(%d+)%s+(%d+)%s+([%a,]+)")
        if first then
            local pressed = {}
            for name in buttons:gmatch("[%a]+") do pressed[name] = true end
            schedule[#schedule + 1] = {tonumber(first), tonumber(last), pressed}
        end
    end
end

local mailbox = assert(io.open(output .. "/mailbox.csv", "w"))
mailbox:write("frame,magic\n")

local function write_file(name, data)
    local file = assert(io.open(output .. "/" .. name, "wb"))
    file:write(data)
    file:close()
end

local function dump(memtype, size)
    local bytes = {}
    for address = 0, size - 1 do bytes[#bytes + 1] = string.char(emu.read(address, memtype)) end
    return table.concat(bytes)
end

emu.addEventCallback(function()
    local pressed = {}
    for _, entry in ipairs(schedule) do
        if frames + 1 >= entry[1] and frames + 1 <= entry[2] then
            for name in pairs(entry[3]) do pressed[name] = true end
        end
    end
    emu.setInput(pressed, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frames = frames + 1
    local magic = emu.read(0x21, emu.memType.nesDebug)
    mailbox:write(string.format("%d,%d\n", frames, magic))
    if frames > startup and magic == 0xFC then valid = valid + 1 end
    if peeks then
        for address = 0, 0x2fff do emu.read(address, emu.memType.nesPpuDebug) end
    end
    if frames == limit then
        mailbox:close()
        write_file("final.png", emu.takeScreenshot())
        local pixels = {}
        for _, value in ipairs(emu.getScreenBuffer()) do pixels[#pixels + 1] = string.pack("<I4", value) end
        write_file("final.argb", table.concat(pixels))
        write_file("palette.bin", dump(emu.memType.nesPaletteRam, 32))
        if dump_prg then write_file("prg.bin", dump(emu.memType.nesPrgRom, 0x8000)) end
        write_file("lua_result.json", string.format(
            '{"frames":%d,"valid_mailboxes_after_startup":%d}\n', frames, valid))
        emu.stop(0)
    end
end, emu.eventType.endFrame)
