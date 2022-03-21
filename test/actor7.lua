local sleep_for = require('sleep_for')

if _CONTEXT == 'main' then
    ch = spawn_vm('.')
    sleep_for(100)
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
