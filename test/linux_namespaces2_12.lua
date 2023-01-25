-- serialization/bad
local spawn_vm = require('./linux_namespaces_libspawn').spawn_vm
local badinjector = require 'linux_namespaces_badinjector'
local sleep_for = require 'sleep_for'

local guest_code = [[
    local sleep_for = require 'sleep_for'
    local inbox = require 'inbox'

    local f = spawn(function()
        inbox:receive()
    end)
    sleep_for(100)
    f:interrupt()
    f:join()

    sleep_for(200)
    print('RECEIVED:', inbox:receive())
]]

local my_channel = spawn_vm(guest_code)
sleep_for(200)
badinjector.send_missing_root_actorfd(my_channel)
my_channel:close()
sleep_for(300) --< wait for some time before we kill the container
