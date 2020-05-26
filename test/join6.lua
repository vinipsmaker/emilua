-- This test ensures join() propagates values back from the joinee.

local println = require('println')
local sleep_for = require('sleep_for')

local fib = spawn(function()
    println('secondary fiber starts')
    sleep_for(20)
    error('tag', 0)
end)

println('secondary fiber spawned')
sleep_for(10)
local ok, e = pcall(function() return fib:join() end)
println(tostring(ok))
println(e)
println('end of main fiber')
