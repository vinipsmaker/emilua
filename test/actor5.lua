-- recv() called on empty incoming

local sleep_for = require('sleep_for')
local println = require('println')

if _CONTEXT == 'main' then
    local ch = spawn_vm('.')
    sleep_for(100)
    ch:send('foo')
    println('bar')
else
    assert(_CONTEXT == 'worker')
    local inbox = require('inbox')
    println(inbox:recv())
end
