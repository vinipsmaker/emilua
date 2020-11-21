local json = require('json')
local inbox = require('inbox')

local body = {
    ignored = function() end,
    foo = {
        "foo1",
        {
            bar2 = "tsarstat",
            [ false ] = "whatever"
        },
        2
    },
    ignored2 = function() end,
    bar = {
        bar1b = "ienoenoin",
        [ true ] = "more whatever"
    },
    ignored3 = function() end,
    qux = {
    },
    ignored4 = function() end
}

if _CONTEXT == 'main' then
    local ch1 = spawn_vm('.')
    local ch2 = spawn_vm('.')

    ch1:send{ from = ch2, body = body }
else assert(_CONTEXT == 'worker')
    local m = inbox:recv()

    if m.from then
        m.from:send{ body = m.body }
    else
        print(json.encode(m.body.foo) == json.encode(body.foo))
        print(json.encode(m.body.bar) == json.encode(body.bar))
        print(json.encode(m.body.qux) == json.encode(body.qux))
        print(m.body.foo[3])
    end
end
