local sleep_for = require('sleep_for')
local sys = require('sys')

local numbers = {8, 42, 38, 111, 2, 39, 1}

local sleeper = spawn(function()
    local children = {}
    scope_cleanup_push(function()
        for _, f in pairs(children) do
            f:interrupt()
        end
    end)
    for _, n in pairs(numbers) do
        children[#children + 1] = spawn(function()
            sleep_for(n)
            print(n)
        end)
    end
    for _, f in pairs(children) do
        f:join()
    end
end)

local sigwaiter = spawn(function()
    local set = sys.signal.set.new(sys.signal.SIGTERM, sys.signal.SIGINT)
    if sys.signal.SIGUSR1 then
        set:add(sys.signal.SIGUSR1)
    end
    set:wait()
    sleeper:interrupt()
end)

sleeper:join()
sigwaiter:interrupt()
