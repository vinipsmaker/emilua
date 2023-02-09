-- receive() called on filled incoming

local sleep_for = require('sleep_for')

if _CONTEXT == 'main' then
    local ch = spawn_vm('.')
    ch:send('foo')
    print('bar')
else
    assert(_CONTEXT == 'worker')
    local inbox = require('inbox')
    sleep_for(0.1)
    print(inbox:receive())
end
