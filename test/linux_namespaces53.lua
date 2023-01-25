-- serialization/bad
local spawn_vm = require('./linux_namespaces_libspawn').spawn_vm

local function gen_oversized_table()
    local ret = {}
    for i = 1, 2000 do
        ret[i .. ''] = '.'
    end
    return ret
end

local my_channel = spawn_vm('')
my_channel:send(gen_oversized_table())
