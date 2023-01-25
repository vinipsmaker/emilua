-- serialization/good
local spawn_vm = require('./linux_namespaces_libspawn').spawn_vm
local inbox = require 'inbox'

local guest_code = [[
    local sleep_for = require 'sleep_for'
    local inbox = require 'inbox'
    local ip = require 'ip'

    local ch = inbox:receive()
    sleep_for(100)
    ch:send{ value = ip.host_name() }
]]

local my_channel = spawn_vm(guest_code)

my_channel:send(inbox)
print(inbox:receive().value)
