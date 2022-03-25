local byte_span = require 'byte_span'
local regex = require 'regex'

local re = regex.new{
    pattern = 'a',
    grammar = 'extended'
}

print(regex.match(re, ''))
print(regex.match(re, 'a'))
print(regex.match(re, 'a '))
print(regex.match(re, ' a'))

print(regex.match(re, byte_span.new(0)))
print(regex.match(re, byte_span.append('a')))
print(regex.match(re, byte_span.append('a ')))
print(regex.match(re, byte_span.append(' a')))

re = regex.new{
    pattern = 'a.*',
    grammar = 'extended'
}

print(regex.match(re, ''))
print(regex.match(re, 'a'))
print(regex.match(re, 'a '))
print(regex.match(re, ' a'))
