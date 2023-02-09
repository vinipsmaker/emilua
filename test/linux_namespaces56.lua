-- close() doesn't discard data already sent
local spawn_vm = require('./linux_namespaces_libspawn').spawn_vm
local sleep = require 'sleep'

local guest_code = [[
    local inbox = require 'inbox'
    print(inbox:receive())
]]

local my_channel = spawn_vm(guest_code)
my_channel:send('hello')
my_channel:close()
sleep(0.3) --< wait for some time before we kill the container
