-- serialization/good
local spawn_vm = require('./linux_namespaces_libspawn').spawn_vm
local sleep_for = require 'sleep_for'
local inbox = require 'inbox'

local guest_code = [[
    local sleep_for = require 'sleep_for'
    local inbox = require 'inbox'

    local msg = inbox:receive()
    local ch = msg.dest
    sleep_for(0.2)
    ch:send{ value = msg.value }
]]

local my_channel = spawn_vm(guest_code)

my_channel:send{ dest = inbox, value = false }

local f = spawn(function()
    inbox:receive()
end)
sleep_for(0.1)
f:interrupt()
f:join()

sleep_for(0.2)
print(inbox:receive().value)
