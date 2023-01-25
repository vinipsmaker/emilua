-- serialization/bad
local spawn_vm = require('./linux_namespaces_libspawn').spawn_vm

local my_channel = spawn_vm('')
my_channel:send({})
