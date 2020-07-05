-- coroutine.resume() is an interruption point, but only before executing the
-- argument
local println = require('println')

f = spawn(function()
    coroutine.wrap(function() end)()
    println('foo')
end)

f:interrupt()
