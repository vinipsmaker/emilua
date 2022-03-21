local inbox = require('inbox')

assert(_CONTEXT == 'worker')
local m = inbox:receive()
m.from:send(m.body)
