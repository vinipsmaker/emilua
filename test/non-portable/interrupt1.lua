fib = spawn(function()
    -- A different approach would be possible where we don't even start the
    -- fiber by detecting it was already interrupted just before we're running
    -- it for the first time, but I opted for the simpler implementation. Any of
    -- these two approaches would be correct thou. Someone's else implementation
    -- could differ and still be correct. Do not make your code depend on the
    -- behaviour of this test.
    print('foo')
    coroutine.wrap(function() this_fiber.yield() end)()
    print('bar')
end)

fib:interrupt()
fib:detach()
