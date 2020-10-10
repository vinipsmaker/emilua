local println = require('println')
local sleep_for = require('sleep_for')

if _CONTEXT == 'main' then
    local ch = spawn_vm('.')
    local f = spawn(function()
        ch:send('foobar')
    end)
    sleep_for(100)
    f:interrupt()
    f:join()
    println('A')
    println(tostring(f.interruption_caught))
else assert(_CONTEXT == 'worker')
    sleep_for(200)
    println('B')
end
