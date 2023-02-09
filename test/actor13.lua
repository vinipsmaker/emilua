local sleep = require('time').sleep

if _CONTEXT == 'main' then
    ch = spawn_vm('.')
    ch:close()
    sleep(0.1)
    ch:send('foobar')
else assert(_CONTEXT == 'worker')
    local inbox = require('inbox')
    inbox:receive()
end
