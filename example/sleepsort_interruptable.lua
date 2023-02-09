local sleep = require('time').sleep
local system = require('system')

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
            sleep(n)
            print(n)
        end)
    end
    for _, f in pairs(children) do
        f:join()
    end
end)

local sigwaiter = spawn(function()
    local set = system.signal.set.new(
        system.signal.SIGTERM, system.signal.SIGINT)
    if system.signal.SIGUSR1 then
        set:add(system.signal.SIGUSR1)
    end
    set:wait()
    sleeper:interrupt()
end)

sleeper:join()
sigwaiter:interrupt()
