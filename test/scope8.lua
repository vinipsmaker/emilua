-- Cleanup handlers that suspend

local println = require('println')

spawn(function()
    scope_cleanup_push(function()
        println('foo1')
        this_fiber.yield()
        println('foo2')
    end)
    error('foo3')
end):detach()

spawn(function()
    scope_cleanup_push(function()
        println('bar1')
        this_fiber.yield()
        println('bar2')
    end)
    error('bar3')
end):detach()
