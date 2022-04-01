local bs = byte_span.new(1, 5)
print(#bs)
print(bs.capacity)

local bs2 = bs:slice()
print(#bs2)
print(bs2.capacity)

bs2 = bs:slice(2)
print(#bs2)
print(bs2.capacity)

bs2 = bs2:slice(1, 4)
print(#bs2)
print(bs2.capacity)

bs2 = bs:slice(1, 5)
print(#bs2)
print(bs2.capacity)

bs2[1] = 20
print(bs2[1])
print(bs[1])

bs2[1] = 21
print(bs2[1])
print(bs[1])

bs2[2] = 22
print(bs2[2])
print(bs[1])

print(bs == bs2:slice(1, 1))
print(bs == bs2)
print(bs:slice(2) == bs2:slice(2, 1))
print(bs:slice(1, 2) == bs2:slice(2, 3))
