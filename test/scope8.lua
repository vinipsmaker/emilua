-- Cleanup handlers that suspend

spawn(function()
    scope_cleanup_push(function()
        print('foo1')
        this_fiber.yield()
        print('foo2')
    end)
    error('foo3')
end):detach()

spawn(function()
    scope_cleanup_push(function()
        print('bar1')
        this_fiber.yield()
        print('bar2')
    end)
    error('bar3')
end):detach()
