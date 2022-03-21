local inbox = require('inbox')

if _CONTEXT == 'main' then
    local ch1 = spawn_vm('.')
    local ch2 = spawn_vm('.')

    ch1:send{ ch2, 'foobar', function()end, 33, 44 }
else assert(_CONTEXT == 'worker')
    local m = inbox:receive()

    if getmetatable(m[1]) == 'tx-channel' then
        local ch = table.remove(m, 1)
        ch:send(m)
    else
        local json = require('json')
        print(json.encode(m))
    end
end
