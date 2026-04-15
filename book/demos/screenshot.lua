-- screenshot.lua
-- mGBA screenshot helper.
--
-- Usage (GUI mGBA, launched by screenshot.bat):
--   mGBA.exe --script screenshot.lua rom.elf
--
-- Takes a screenshot after 59 rendered frames. mGBA saves the file
-- next to the ROM. The calling process kills mGBA afterwards.

local frames = 0

callbacks:add("frame", function()
    frames = frames + 1
    if frames == 59 then
        emu:screenshot()
        console:log("screenshot saved")
    end
end)
