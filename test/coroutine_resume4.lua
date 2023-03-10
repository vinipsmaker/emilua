-- That's similar to LuaJIT's CORUN error. The runtime can further explore this
-- restriction (user can no longer do shenanigans to wake up not-ready fibers)
-- to make implementation way easier.
shared = coroutine.create(function() this_fiber.yield() end)

spawn(function()
    -- Conceptually, coroutine `shared` is still running when this fiber
    -- suspends. Therefore, it is only natural to make the implementation follow
    -- the concept.
    coroutine.resume(shared)
end)

print(coroutine.status(shared))
this_fiber.yield()
print(coroutine.status(shared))
coroutine.resume(shared)
