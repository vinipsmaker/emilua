local byte_span = require 'byte_span'

-- cannot have end beyond capacity
local bs = byte_span.new(1, 3):slice(1, 4)
print(#bs)
print(bs.capacity)
