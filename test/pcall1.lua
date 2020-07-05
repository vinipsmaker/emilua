-- pcall() is an interruption point, but only before executing the argument
local println = require('println')

f = spawn(function()
    pcall(function() end)
    println('foo')
end)

f:interrupt()
