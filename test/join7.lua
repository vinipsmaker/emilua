-- This test ensures join() detects EDEADLK

fib = spawn(function()
    fib:join()
end)
