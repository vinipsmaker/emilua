-- Read invalid interruption_caught on not-yet-had-a-chance-to-run-fiber
fib = spawn(function() end)
print(fib.interruption_caught)
