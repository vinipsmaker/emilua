-- This test ensures that fibers whose joinable fiber handles got collected
-- will have their stacktrace printed to stderr (if err'ed)

collectgarbage("stop")

spawn(function()
    print('secondary fiber starts')
    error('tag', 0)
end)

print('secondary fiber spawned')
this_fiber.yield()
print('end of main fiber')
