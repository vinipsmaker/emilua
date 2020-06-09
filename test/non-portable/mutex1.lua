-- mutex.lock has dispatch semantics

local println = require('println')
local mutex = require('mutex')

spawn(function() println('bar') end)

m = mutex.new()
m:lock()
println('foo')
