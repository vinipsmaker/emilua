local byte_span = require 'byte_span'

local bs = byte_span.new(1, 2):slice(0)
print(#bs)
print(bs.capacity)
