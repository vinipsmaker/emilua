-- Read invalid interruption_caught on fiber who reacted to request
local println = require('println')

fib = spawn(function()
    this_fiber.yield()
end)
fib:interrupt()
this_fiber.yield()
println(tostring(fib.interruption_caught))
