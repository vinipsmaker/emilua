-- serialization/bad
local spawn_vm = require('./linux_namespaces_libspawn').spawn_vm
local badinjector = require 'linux_namespaces_badinjector'
local sleep = require 'sleep'

local guest_code = [[
    local inbox = require 'inbox'
    print('RECEIVED:', inbox:receive())
]]

local my_channel = spawn_vm(guest_code)
sleep(0.1)
badinjector.send_invalid_root(my_channel)
my_channel:close()
sleep(0.3) --< wait for some time before we kill the container
