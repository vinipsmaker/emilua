-- scope() is not an interruption point
--
-- This test is only here because pcall() is an interruption point and scope()
-- is implemented in terms of pcall(). Some silly change could make scope()
-- inherit this undesired property. This test can prevent such break from going
-- unnoticed.
local println = require('println')

f = spawn(function()
    scope(function() end)
    println('foo')
end)

f:interrupt()
