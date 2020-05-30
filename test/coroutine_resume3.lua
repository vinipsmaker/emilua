println = require('println')

coroutine.wrap(function()
    this_fiber.yield()
    println('foo')
end)()

println('bar')
