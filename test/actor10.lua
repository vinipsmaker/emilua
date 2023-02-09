local sleep = require('time').sleep

if _CONTEXT == 'main' then
    ch = spawn_vm('.')
    sleep(0.1)
else assert(_CONTEXT == 'worker')
    local inbox = require('inbox')
    spawn(function()
        inbox:close()
    end):detach()
    inbox:receive()
end
