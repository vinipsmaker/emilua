println = require('println')

coroutine.resume(coroutine.create(function()
    this_fiber.yield()
    println('foo')
end))

println('bar')
