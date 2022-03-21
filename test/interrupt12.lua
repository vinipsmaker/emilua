-- Read invalid interruption_caught on some detached fiber
fib = spawn(function()
    this_fiber.yield()
end)
this_fiber.yield()
fib:detach()
print(fib.interruption_caught)
