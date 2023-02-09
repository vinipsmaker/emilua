-- receive() called on filled incoming

local sleep = require('time').sleep

if _CONTEXT == 'main' then
    local ch = spawn_vm('.')
    ch:send('foo')
    print('bar')
else
    assert(_CONTEXT == 'worker')
    local inbox = require('inbox')
    sleep(0.1)
    print(inbox:receive())
end
