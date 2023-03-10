-- serialization/good
local spawn_vm = require('./linux_namespaces_libspawn').spawn_vm
local inbox = require 'inbox'

local guest_code = [[
    local sleep = require('time').sleep
    local inbox = require 'inbox'
    local ip = require 'ip'

    local ch = inbox:receive()
    sleep(0.1)
    ch:send(ip.host_name())
]]

local my_channel = spawn_vm(guest_code)

my_channel:send(inbox)
print(inbox:receive())
