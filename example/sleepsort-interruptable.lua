local sleep_for = require('sleep_for')

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
    local sigusr1 = signals.new(signals.SIGUSR1)
    sigusr1:wait()
    sleeper:interrupt()
end)

sleeper:join()
sigwaiter:interrupt()
