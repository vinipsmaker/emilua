local inbox = require('inbox')

if _CONTEXT == 'main' then
    local ch1 = spawn_vm('.')
    local ch2 = spawn_vm('.')

    ch1:send{ from = ch2, body = 'Hello World' }
else assert(_CONTEXT == 'worker')
    local m = inbox:receive()

    if m.from then
        m.from:send{ body = m.body }
    else
        print(m.body)
    end
end
