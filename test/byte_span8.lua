-- capacity=0
local bs = byte_span.new(1, 2):slice(1, 2):slice(3)
print(#bs)
print(bs.capacity)
