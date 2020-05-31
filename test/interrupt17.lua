-- This test ensures joiner's interrupter has been cleared by the join algorithm
-- and won't be reused again as an interrupter for another operation

local println = require('println')
local sleep_for = require('sleep_for')

fib = spawn(function()
    spawn(function() end):join()
    spawn(function() fib:interrupt() end):detach()
    this_fiber.disable_interruption()
    spawn(function() sleep_for(10) println('foo') end):detach()
    sleep_for(20)
    println('bar')
end)
