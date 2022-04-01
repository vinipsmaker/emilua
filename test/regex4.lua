local regex = require 'regex'

local re = regex.new{
    pattern = ',',
    grammar = 'basic'
}

local s = 'Robbins,Arnold,"1234 A Pretty Street, NE",MyTown,MyState,12345-6789,USA'

for k, v in ipairs(regex.split(re, s)) do
    print(k, v)
end

s = byte_span.append(s)

for k, v in ipairs(regex.split(re, s)) do
    print(k, v)
end
