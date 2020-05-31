local println = require('println')

fib = spawn(function()
    println('foo')
    coroutine.wrap(function() this_fiber.yield() end)()
    println('bar')
end)

fib:interrupt()
this_fiber.yield()
fib:detach()
