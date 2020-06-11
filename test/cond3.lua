-- PTHREAD_MUTEX_ERRORCHECK subset

local println = require('println')
local mutex = require('mutex')
local cond = require('cond')

local m = mutex.new()
local c = cond.new()

c:wait(m)
