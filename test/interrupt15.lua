-- Disable/restore interruption

local println = require('println')

fib = spawn(function()
    fib:interrupt()
    println('foo')
    this_fiber.disable_interruption()
    this_fiber.yield()
    println('bar')
    this_fiber.restore_interruption()
    -- Yes, 'baz' will still be printed. We DO have to wait until a new
    -- interruption is reached before we can raise `fiber_interrupted`. Consider
    -- the following: how would you implement an op whose some critical section
    -- cannot be interrupted? Right, `disable_interruption()`. But once you
    -- restore interruption, the result might be ready already, and the proper
    -- behaviour is to deliver it to the user so it can do something with it. If
    -- `restore_interruption()` itself was an interruption point, this op
    -- function could not be implemented.
    println('baz')
    this_fiber.yield()
    println('qux')
end)
