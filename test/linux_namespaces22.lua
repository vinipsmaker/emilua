-- serialization/good
local spawn_vm2 = require('./linux_namespaces_libspawn').spawn_vm
local sleep_for = require 'sleep_for'
local inbox = require 'inbox'

local guest_code = [[
    local sleep_for = require 'sleep_for'
    local inbox = require 'inbox'

    local f = spawn(function()
        inbox:receive()
    end)
    sleep_for(0.1)
    f:interrupt()
    f:join()

    sleep_for(0.2)
    local ch = inbox:receive().value
    ch:send('hello')
]]

if _CONTEXT == 'main' then
    local container = spawn_vm2(guest_code)
    local actor = spawn_vm('.')

    actor:send(inbox)
    inbox:receive() --< sync
    sleep_for(0.2)
    container:send{ value = actor }
    actor:close()
    print(inbox:receive())
else
    local master = inbox:receive()
    master:send('started')
    master:send(inbox:receive())
end
