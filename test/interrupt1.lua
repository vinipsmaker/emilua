-- Self-interrupt
--
-- Do notice that Emilua doesn't expose vocabulary to express
-- pthread_cancel(pthread_self()). This is intentional.
--
-- If `interrupted` is explicitly raised from the main fiber by user code, a
-- stack trace will be printed just like usual (but this logging behaviour might
-- change back and forth between versions as my mood changes).

local println = require('println')

fib = spawn(function()
    fib:interrupt()
    println('foo')
    coroutine.wrap(function() this_fiber.yield() end)()
    println('bar')
end)
