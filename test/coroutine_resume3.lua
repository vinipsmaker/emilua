coroutine.wrap(function()
    this_fiber.yield()
    print('foo')
end)()

print('bar')
