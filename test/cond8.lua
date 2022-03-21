-- Interrupt

local mutex = require('mutex')
local cond = require('cond')

local m = mutex.new()
local c = cond.new()

f = spawn(function()
    m:lock()
    local ok, err = pcall(function() c:wait(m) end)
    print(ok)
    m:unlock()
    if not ok then
        error(err, 0)
    end
end)

this_fiber.yield()
f:interrupt()
f:join()
print(f.interruption_caught)
