-- Deadlock detection

local mutex = require('mutex')

m = mutex.new()
m:lock()
m:lock()
