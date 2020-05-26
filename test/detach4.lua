-- This test ensures that detached fibers who error will have their stacktrace
-- printed to stderr

local println = require('println')

spawn(function()
    println('secondary fiber starts')
    error('tag', 0)
end):detach()

println('secondary fiber spawned')
this_fiber.yield()
println('end of main fiber')
