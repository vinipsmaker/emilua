local byte_span = require 'byte_span'

-- cannot have start beyond end
local bs = byte_span.new(1, 3):slice(3, 1)
print(#bs)
print(bs.capacity)
