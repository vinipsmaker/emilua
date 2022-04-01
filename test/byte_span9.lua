-- cannot have length=-1
local bs = byte_span.new(1, 3):slice(3)
print(#bs)
print(bs.capacity)
