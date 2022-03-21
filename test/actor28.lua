local inbox = require('inbox')

local ch = spawn_vm('../actor28_foo')
ch:send{ from = inbox, body = 'Hello World' }
local m = inbox:recv()
print(m)
