-- pcall() is an interruption point, but only before executing the argument
f = spawn(function()
    pcall(function() end)
    print('foo')
end)

f:interrupt()
