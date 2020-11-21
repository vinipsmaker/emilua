local inbox = require('inbox')

if _CONTEXT == 'main' then
    local ch1 = spawn_vm('.')
    local ch2 = spawn_vm('.')

    local body = {}
    setmetatable(body, {})
    ch1:send{ from = ch2, body = body }
else assert(_CONTEXT == 'worker')
    local m = inbox:recv()

    if m.from then
        m.from:send{ body = m.body }
    else
        print(m.body)
    end
end
