local bs = byte_span.append('BCDEF')

print(bs:find_first_not_of('ABC'))
print(bs:find_first_not_of('ABC', 5))
print(bs:find_first_not_of('B'))
print(bs:find_first_not_of('D', 3))
