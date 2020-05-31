-- Join on fiber that hasn't reacted to an interruption request yet
local println = require('println')

fib = spawn(function()
    this_fiber.yield()
end)
fib:interrupt()
fib:join()
println(tostring(fib.interruption_caught))
