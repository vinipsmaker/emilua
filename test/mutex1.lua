-- Given Emilua's lock() uses dispatch semantics, a separate test just to ensure
-- check-suspend still happens make sense

local mutex = require('mutex')

m = mutex.new()
this_fiber.forbid_suspend()
m:lock()
