local println = require('println')

fib = spawn(function()
    fib:interrupt()
    fib:detach()
    println('foo')
    coroutine.wrap(function() this_fiber.yield() end)()
    println('bar')
end)
