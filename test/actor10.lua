local sleep_for = require('sleep_for')

if _CONTEXT == 'main' then
    ch = spawn_vm('.')
    sleep_for(100)
else assert(_CONTEXT == 'worker')
    local inbox = require('inbox')
    spawn(function()
        inbox:close()
    end):detach()
    inbox:receive()
end
