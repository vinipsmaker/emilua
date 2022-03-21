coroutine.resume(coroutine.create(function()
    this_fiber.yield()
    print('foo')
end))

print('bar')
