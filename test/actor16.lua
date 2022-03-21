local inbox = require('inbox')

if _CONTEXT == 'main' then
    local ch = spawn_vm('.')
    ch:send(inbox)
    ch:send(inbox)
    ch:send(ch)
else assert(_CONTEXT == 'worker')
    local a = inbox:recv()
    local b = inbox:recv()
    local c = inbox:recv()
    print(a == b)
    print(b == c)
end
