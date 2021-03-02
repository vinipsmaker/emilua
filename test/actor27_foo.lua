local inbox = require('inbox')

assert(_CONTEXT == 'worker')
local m = inbox:recv()
m.from:send(m.body)
