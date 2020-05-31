-- Read invalid interruption_caught on some fiber
local println = require('println')

fib = spawn(function()
    this_fiber.yield()
end)
this_fiber.yield()
println(tostring(fib.interruption_caught))
