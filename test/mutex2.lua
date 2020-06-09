-- Emilua's mutex is a subset of PTHREAD_MUTEX_ERRORCHECK
-- Therefore, EPERM should be raised here

local mutex = require('mutex')

m = mutex.new()
m:unlock()
