-- Read invalid interruption_caught on detached finished fiber
local println = require('println')

fib = spawn(function() end)
this_fiber.yield()
fib:detach()
println(tostring(fib.interruption_caught))
