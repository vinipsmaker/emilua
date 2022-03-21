-- coroutine.resume() is an interruption point, but only before executing the
-- argument
f = spawn(function()
    coroutine.wrap(function() end)()
    print('foo')
end)

f:interrupt()
