-- Join on fiber that hasn't reacted to an interruption request yet
fib = spawn(function()
    this_fiber.yield()
end)
fib:interrupt()
fib:join()
print(fib.interruption_caught)
