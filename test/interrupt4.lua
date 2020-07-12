-- interrupt() while join()'ing

local println = require('println')
local sleep_for = require('sleep_for')

f = spawn(function()
    sleep_for(10)
    println('foo')
end)

spawn(function() f:interrupt() end):detach()
f:join()
println(tostring(f.interruption_caught))
