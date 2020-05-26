-- This test ensures that detached fibers who error will have their stacktrace
-- printed to stderr

local println = require('println')
local sleep_for = require('sleep_for')

local fib = spawn(function()
    println('secondary fiber starts')
    error('tag', 0)
end)

println('secondary fiber spawned')
sleep_for(10)
fib:detach()
println('end of main fiber')
