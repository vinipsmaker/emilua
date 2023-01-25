-- serialization/bad
local spawn_vm = require('./linux_namespaces_libspawn').spawn_vm
local badinjector = require 'linux_namespaces_badinjector'

local function gen_oversized_table()
    local ret = {}
    for i = 1, badinjector.CONFIG_MESSAGE_MAX_MEMBERS_NUMBER + 1 do
        ret[i .. ''] = '.'
    end
    return ret
end

local my_channel = spawn_vm('')
my_channel:send(gen_oversized_table())
