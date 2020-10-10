-- Test if `inbox:close()` works on active VMs

local sleep_for = require('sleep_for')

if _CONTEXT == 'main' then
    local ch = spawn_vm('.')
    ch:send('foobar')
else assert(_CONTEXT == 'worker')
    local inbox = require('inbox')
    inbox:close()
    sleep_for(100)
end
