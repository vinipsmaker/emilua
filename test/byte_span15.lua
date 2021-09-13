local byte_span = require 'byte_span'

local bs = byte_span.new(4, 5)
bs:slice(1, 5)[5] = 33
bs:copy('hello')
print(#bs)
print(bs.capacity)
print(bs)
bs = bs:slice(1, 5)
print(#bs)
print(bs.capacity)
print(bs)
bs:copy('hello')
print(bs)
bs:slice(2):copy(bs)
print(bs)
bs:copy(bs:slice(2))
print(bs)
