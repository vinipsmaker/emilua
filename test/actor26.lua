local json = require('json')

if _CONTEXT == 'main' then
    local ch1 = spawn_vm('.')
    local ch2 = spawn_vm('.')

    local a = {}
    ch1:send{ to = ch2, body = { a, a } }
else assert(_CONTEXT == 'worker')
    local inbox = require('inbox')
    local m = inbox:recv()

    if m.to then
        m.to:send{ body = m.body }
    else
        print(json.encode(m.body))
    end
end
