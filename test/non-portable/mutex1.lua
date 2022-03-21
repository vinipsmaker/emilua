-- mutex.lock has dispatch semantics

local mutex = require('mutex')

spawn(function() print('bar') end)

m = mutex.new()
m:lock()
print('foo')
