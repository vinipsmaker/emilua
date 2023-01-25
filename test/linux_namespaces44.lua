-- serialization/good
local spawn_vm2 = require('./linux_namespaces_libspawn').spawn_vm
local sleep_for = require 'sleep_for'
local inbox = require 'inbox'

local guest_code = [[
    local inbox = require 'inbox'

    local ch = inbox:receive().value
    ch:send('hello')
]]

if _CONTEXT == 'main' then
    local container = spawn_vm2(guest_code)
    local actor = spawn_vm('.')

    actor:send(inbox)
    inbox:receive() --< sync
    sleep_for(100)
    container:send{ value = actor }
    actor:close()
    print(inbox:receive())
else
    local master = inbox:receive()
    master:send('started')
    master:send(inbox:receive())
end
