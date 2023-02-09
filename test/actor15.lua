local sleep = require('sleep')

if _CONTEXT == 'main' then
    local ch = spawn_vm('.')
    local f = spawn(function()
        ch:send('foobar')
    end)
    sleep(0.1)
    f:interrupt()
    f:join()
    print('A')
    print(f.interruption_caught)
else assert(_CONTEXT == 'worker')
    sleep(0.2)
    print('B')
end
