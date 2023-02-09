-- serialization/good
local spawn_vm = require('./linux_namespaces_libspawn').spawn_vm
local sleep_for = require 'sleep_for'
local stream = require 'stream'
local pipe = require 'pipe'

local guest_code = [[
    local sleep_for = require 'sleep_for'
    local stream = require 'stream'
    local inbox = require 'inbox'
    local pipe = require 'pipe'

    local f = spawn(function()
        inbox:receive()
    end)
    sleep_for(0.1)
    f:interrupt()
    f:join()

    sleep_for(0.2)
    local pout = pipe.write_stream.new(inbox:receive())
    stream.write_all(pout, 'test')
]]

local pin, pout = pipe.pair()
pout = pout:release()

local my_channel = spawn_vm(guest_code)

sleep_for(0.2)
my_channel:send(pout)
pout:close()
local buf = byte_span.new(4)
stream.read_all(pin, buf)
print(buf)
