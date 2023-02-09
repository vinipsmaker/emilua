local sleep_for = require('sleep_for')

if _CONTEXT == 'main' then
    ch = spawn_vm('.')
    ch:close()
    sleep_for(0.1)
    ch:send('foobar')
else assert(_CONTEXT == 'worker')
    local inbox = require('inbox')
    inbox:receive()
end
