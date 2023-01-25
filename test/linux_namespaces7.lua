-- serialization/good
local spawn_vm = require('./linux_namespaces_libspawn').spawn_vm
local sleep_for = require 'sleep_for'
local inbox = require 'inbox'

local guest_code = [[
    local sleep_for = require 'sleep_for'
    local inbox = require 'inbox'

    local msg = inbox:receive()
    local ch = msg.dest
    sleep_for(200)
    ch:send(msg.value)
]]

local my_channel = spawn_vm(guest_code)

my_channel:send{ dest = inbox, value = 0 }

local f = spawn(function()
    inbox:receive()
end)
sleep_for(100)
f:interrupt()
f:join()

sleep_for(200)
print(inbox:receive())
