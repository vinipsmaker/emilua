local sleep = require('time').sleep

if _CONTEXT == 'main' then
    local ch = spawn_vm('.')
    local f = spawn(function()
        ch:send('foobar')
    end)
    sleep(0.1)
    f:interrupt()
    local ok, e = pcall(function() f:join() end)
    print(f.interruption_caught)
    error(e)
else assert(_CONTEXT == 'worker')
    spawn(function()
        sleep(0.2)
    end):detach()
end
