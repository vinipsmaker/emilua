local byte_span = require 'byte_span'

local bs = byte_span.new(0)
print(#bs)
print(bs.capacity)
