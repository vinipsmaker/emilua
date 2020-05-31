-- Join on fiber that already reacted to an interruption request
local println = require('println')

fib = spawn(function()
    this_fiber.yield()
end)
fib:interrupt()
this_fiber.yield()
fib:join()
println(tostring(fib.interruption_caught))
