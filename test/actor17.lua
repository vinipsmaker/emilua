local inbox = require('inbox')

if _CONTEXT == 'main' then
    local ch1 = spawn_vm('.')
    local ch2 = spawn_vm('.')

    ch1:send{ch2, 'Hello', 'World'}
else assert(_CONTEXT == 'worker')
    local m = inbox:recv()

    if type(m) == 'table' then
        m[1]:send(m[2] .. ' ' .. m[3])
    else
        print(m)
    end
end
