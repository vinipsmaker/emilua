-- This test ensures join() propagates values back from the joinee.

local println = require('println')
local sleep_for = require('sleep_for')

local fib = spawn(function()
    println('secondary fiber starts')
    sleep_for(20)
    return 30, 31, 32
end)

println('secondary fiber spawned')
sleep_for(10)
local ret = {fib:join()}
println(tostring(#ret))
println(tostring(ret[1]))
println(tostring(ret[2]))
println(tostring(ret[3]))
println('end of main fiber')
