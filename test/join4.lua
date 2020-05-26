-- This test ensures join() propagates values back from the joinee.

local println = require('println')

local fib = spawn(function()
    println('secondary fiber starts')
    error('tag', 0)
end)

println('secondary fiber spawned')
this_fiber.yield()
local ok, e = pcall(function() return fib:join() end)
println(tostring(ok))
println(e)
println('end of main fiber')
