local inbox = require('inbox')

if _CONTEXT == 'main' then
    local ch = spawn_vm('.')
    ch:send(inbox)
    ch:send(inbox)
    ch:send(ch)
else assert(_CONTEXT == 'worker')
    local a = inbox:receive()
    local b = inbox:receive()
    local c = inbox:receive()
    print(a == b)
    print(b == c)
end
