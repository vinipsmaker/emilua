-- This test ensures that detached fibers who error will have their stacktrace
-- printed to stderr

local println = require('println')

local fib = spawn(function()
    println('secondary fiber starts')
    error('tag', 0)
end)

println('secondary fiber spawned')
this_fiber.yield()
fib:detach()
println('end of main fiber')
