-- This test ensures joiner's interrupter has been cleared by the join algorithm
-- and won't be reused again as an interrupter for another operation

local sleep = require('sleep')

fib = spawn(function()
    spawn(function() end):join()
    spawn(function() fib:interrupt() end):detach()
    this_fiber.disable_interruption()
    spawn(function() sleep(0.01) print('foo') end):detach()
    sleep(0.02)
    print('bar')
end)
