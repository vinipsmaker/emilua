-- pcall() is an interruption point, but only before executing the argument
f = spawn(function()
    xpcall(function() end, function() end)
    print('foo')
end)

f:interrupt()
