-- This test ensures that detached fibers who error will have their stacktrace
-- printed to stderr

spawn(function()
    print('secondary fiber starts')
    error('tag', 0)
end):detach()

print('secondary fiber spawned')
this_fiber.yield()
print('end of main fiber')
