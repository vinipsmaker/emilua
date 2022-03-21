-- This test ensures that detached fibers who error will have their stacktrace
-- printed to stderr

local fib = spawn(function()
    print('secondary fiber starts')
    error('tag', 0)
end)

print('secondary fiber spawned')
this_fiber.yield()
fib:detach()
print('end of main fiber')
