-- The interrupt call is always valid

f = spawn(function() end)
f:interrupt()
f:detach()
f:interrupt()
f:interrupt()

f = spawn(function() end)
f:interrupt()
f:join()
f:interrupt()
f:interrupt()
