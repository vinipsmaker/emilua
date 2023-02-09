local sleep = require('time').sleep

if _CONTEXT == 'main' then
    ch = spawn_vm('.')
    sleep(0.1)
    ch:send('foo')
    print('bar')
else
    assert(_CONTEXT == 'worker')
    local inbox = require('inbox')
    spawn(function()
        inbox:receive()
    end):detach()
    print(inbox:receive())
end
