-- serialization/good
local spawn_vm = require('./linux_namespaces_libspawn').spawn_vm
local sleep = require 'sleep'
local stream = require 'stream'
local pipe = require 'pipe'

local guest_code = [[
    local stream = require 'stream'
    local inbox = require 'inbox'
    local pipe = require 'pipe'

    local pout = pipe.write_stream.new(inbox:receive().value)
    stream.write_all(pout, 'test')
]]

local pin, pout = pipe.pair()
pout = pout:release()

local my_channel = spawn_vm(guest_code)

sleep(0.1)
my_channel:send{ value = pout }
pout:close()
local buf = byte_span.new(4)
stream.read_all(pin, buf)
print(buf)
