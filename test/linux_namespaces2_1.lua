-- serialization/good
local spawn_vm = require('./linux_namespaces_libspawn').spawn_vm
local badinjector = require 'linux_namespaces_badinjector'
local inbox = require 'inbox'

local guest_code = [[
    local inbox = require 'inbox'

    local ch = inbox:receive()['1']
    ch:send('hello')
]]

local function gen_oversized_table()
    local ret = {}
    for i = 1, badinjector.CONFIG_MESSAGE_MAX_MEMBERS_NUMBER do
        ret[i .. ''] = inbox
    end
    return ret
end

local my_channel = spawn_vm(guest_code)
my_channel:send(gen_oversized_table())
print(inbox:receive())
