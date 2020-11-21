local inbox = require('inbox')

if _CONTEXT == 'main' then
    local ch1 = spawn_vm('.')
    local ch2 = spawn_vm('.')

    ch1:send{ ch2, 'foobar', function()end, 33, 44 }
else assert(_CONTEXT == 'worker')
    local m = inbox:recv()

    if getmetatable(m[1]) == 'tx-channel' then
        local m2 = {}
        for i, v in ipairs(m) do
            if i > 1 then
                m2[i - 1] = v
            end
        end
        m[1]:send(m2)
    else
        local json = require('json')
        print(json.encode(m))
    end
end
