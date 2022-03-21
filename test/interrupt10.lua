-- Read invalid interruption_caught on fiber who reacted to request
fib = spawn(function()
    this_fiber.yield()
end)
fib:interrupt()
this_fiber.yield()
print(fib.interruption_caught)
