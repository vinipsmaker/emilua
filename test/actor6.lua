-- recv() called on filled incoming

local sleep_for = require('sleep_for')
local println = require('println')

if _CONTEXT == 'main' then
    local ch = spawn_vm('.')
    ch:send('foo')
    println('bar')
else
    assert(_CONTEXT == 'worker')
    local inbox = require('inbox')
    sleep_for(100)
    println(inbox:recv())
end
