fib = spawn(function()
    print('foo')
    coroutine.wrap(function() this_fiber.yield() end)()
    print('bar')
end)

fib:interrupt()
this_fiber.yield()
fib:detach()
