-- This test ensures join() propagates values back from the joinee.

local fib = spawn(function()
    print('secondary fiber starts')
    error('tag', 0)
end)

print('secondary fiber spawned')
this_fiber.yield()
local ok, e = pcall(function() return fib:join() end)
print(ok)
print(e)
print('end of main fiber')
