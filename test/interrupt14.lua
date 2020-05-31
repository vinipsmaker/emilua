-- Read invalid interruption_caught on detached fiber who reacted to request
local println = require('println')

fib = spawn(function()
    this_fiber.yield()
end)
fib:interrupt()
this_fiber.yield()
fib:detach()
println(tostring(fib.interruption_caught))
