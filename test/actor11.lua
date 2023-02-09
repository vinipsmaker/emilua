local sleep_for = require('sleep_for')

if _CONTEXT == 'main' then
    local ch = spawn_vm('.')
    local f = spawn(function()
        ch:send('foobar')
    end)
    sleep_for(0.1)
    f:interrupt()
    local ok, e = pcall(function() f:join() end)
    print(f.interruption_caught)
    error(e)
else assert(_CONTEXT == 'worker')
    spawn(function()
        sleep_for(0.2)
    end):detach()
end
