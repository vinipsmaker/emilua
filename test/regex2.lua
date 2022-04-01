local regex = require 'regex'

local re = regex.new{
    pattern = ' (a)?(b)',
    grammar = 'extended'
}

print(regex.match(re, ' b'))
print(regex.match(re, ' ab'))

print(regex.match(re, byte_span.append(' b')))
print(regex.match(re, byte_span.append(' ab')))
