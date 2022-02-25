local ip = require 'ip'
local byte_span = require 'byte_span'
local sleep_for = require 'sleep_for'

local sock = ip.tcp.socket.new()
sock:connect(ip.address.loopback_v4(), 1234)

sleep_for(500)
sock:send(byte_span.append('a'), ip.message_flag.out_of_band)

sleep_for(2000)
sock:write_some(byte_span.append('test'))
