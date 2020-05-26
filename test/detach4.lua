-- This test ensures that detached fibers who error will have their stacktrace
-- printed to stderr

local println = require('println')
local sleep_for = require('sleep_for')

spawn(function()
    println('secondary fiber starts')
    error('tag', 0)
end):detach()

println('secondary fiber spawned')
sleep_for(10)
println('end of main fiber')
