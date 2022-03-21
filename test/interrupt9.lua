-- Read invalid interruption_caught on finished fiber
fib = spawn(function() end)
this_fiber.yield()
print(fib.interruption_caught)
