-- Read invalid interruption_caught on detached fiber who reacted to request
fib = spawn(function()
    this_fiber.yield()
end)
fib:interrupt()
this_fiber.yield()
fib:detach()
print(fib.interruption_caught)
