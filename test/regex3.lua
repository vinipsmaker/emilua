local regex = require 'regex'

local re = regex.new{
    pattern = 'u(ab)(c)?',
    grammar = 'extended'
}

local s = ' uabd'
local results = regex.search(re, s)
print('empty', results.empty)

print(string.sub(s, results[0].start, results[0].end_))
for i = 0, re.mark_count do
    if results[i] then
        print(i, results[i].start, results[i].end_)
    else
        print(i, 'nil')
    end
end

s = byte_span.append(s)
results = regex.search(re, s)
print('empty', results.empty)

print(s:slice(results[0].start, results[0].end_))
for i = 0, re.mark_count do
    if results[i] then
        print(i, results[i].start, results[i].end_)
    else
        print(i, 'nil')
    end
end

print(regex.search(re, 'x').empty)
