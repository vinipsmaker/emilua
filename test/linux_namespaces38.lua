-- serialization/good
local spawn_vm = require('./linux_namespaces_libspawn').spawn_vm
local inbox = require 'inbox'

local guest_code = [[
    local sleep_for = require 'sleep_for'
    local inbox = require 'inbox'

    local msg = inbox:receive()
    local ch = msg.dest
    sleep_for(100)
    ch:send{ value = msg.value }
]]

local my_channel = spawn_vm(guest_code)

my_channel:send{ dest = inbox, value = -1 / 0 }
print(inbox:receive().value)
