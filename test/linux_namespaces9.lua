-- serialization/good
local spawn_vm = require('./linux_namespaces_libspawn').spawn_vm
local sleep = require('time').sleep
local inbox = require 'inbox'

local guest_code = [[
    local sleep = require('time').sleep
    local inbox = require 'inbox'

    local msg = inbox:receive()
    local ch = msg.dest
    sleep(0.2)
    ch:send(msg.value)
]]

local my_channel = spawn_vm(guest_code)

my_channel:send{ dest = inbox, value = 1 }

local f = spawn(function()
    inbox:receive()
end)
sleep(0.1)
f:interrupt()
f:join()

sleep(0.2)
print(inbox:receive())
