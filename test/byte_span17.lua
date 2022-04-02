local bs = byte_span.append(' long long int;')

print(bs:find('long'))
print(bs:find('long', 3))
print(bs:find('long', 6))
print(bs:find(' '))
print(bs:find('o', 2))
print(bs:find('on'))
