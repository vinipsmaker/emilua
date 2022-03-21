-- Read invalid interruption_caught on some fiber
fib = spawn(function()
    this_fiber.yield()
end)
this_fiber.yield()
print(fib.interruption_caught)
