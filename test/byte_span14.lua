-- cannot have end beyond capacity
local bs = byte_span.new(1, 3):slice(2):slice(1, 3)
print(#bs)
print(bs.capacity)
