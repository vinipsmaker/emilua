-- pcall() is an interruption point, but only before executing the argument
local println = require('println')

f = spawn(function()
    xpcall(function() end, function() end)
    println('foo')
end)

f:interrupt()
