local sleep = require('time').sleep

if _CONTEXT == 'main' then
    local inbox = require('inbox')
    local ch = spawn_vm('.')
    spawn(function()
        ch:send(inbox)
    end):detach()
    local f = spawn(function()
        inbox:receive()
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
