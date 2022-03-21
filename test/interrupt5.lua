-- Join on fiber that already reacted to an interruption request
fib = spawn(function()
    this_fiber.yield()
end)
fib:interrupt()
this_fiber.yield()
fib:join()
print(fib.interruption_caught)
