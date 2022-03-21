-- Read invalid interruption_caught on detached
-- not-yet-had-a-chance-to-run-fiber
fib = spawn(function() end)
fib:detach()
print(fib.interruption_caught)
