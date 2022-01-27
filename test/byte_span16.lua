local byte_span = require 'byte_span'

local bs = byte_span.new(1, 3)
print(#bs)
print(bs.capacity)

bs:slice(1, 3):copy('foo')
print(bs)
print(bs:slice(1, 3))

print(bs:append('bar'))
print(bs:slice(1, 3))
print(bs:append('ba'))
print(bs:slice(1, 3))

bs = bs:append('oo')
print(bs:append(' ', bs, ' ', bs))

print(byte_span.append(nil, bs))
