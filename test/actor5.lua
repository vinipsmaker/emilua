-- receive() called on empty incoming

local sleep = require('sleep')

if _CONTEXT == 'main' then
    local ch = spawn_vm('.')
    sleep(0.1)
    ch:send('foo')
    print('bar')
else
    assert(_CONTEXT == 'worker')
    local inbox = require('inbox')
    print(inbox:receive())
end
