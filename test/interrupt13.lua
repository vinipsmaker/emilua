-- Read invalid interruption_caught on detached finished fiber
fib = spawn(function() end)
this_fiber.yield()
fib:detach()
print(fib.interruption_caught)
