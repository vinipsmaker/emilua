-- This test ensures join() detects EDEADLK

fib = spawn(function()
    coroutine.wrap(function()
        fib:join()
    end)()
end)
