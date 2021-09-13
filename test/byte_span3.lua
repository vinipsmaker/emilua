local byte_span = require 'byte_span'

local bs = byte_span.new(0, 1)
print(#bs)
print(bs.capacity)

bs = byte_span.new(1)
print(#bs)
print(bs.capacity)
