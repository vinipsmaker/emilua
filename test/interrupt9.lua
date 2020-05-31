-- Read invalid interruption_caught on finished fiber
local println = require('println')

fib = spawn(function() end)
this_fiber.yield()
println(tostring(fib.interruption_caught))
