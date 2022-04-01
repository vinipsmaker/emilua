-- From GAWK's rationale[1] (check test regex6.lua too):
--
-- > regular expression matching with FPAT can match the null string, and the
-- > non-matching intervening characters function as the separators
--
-- [1] https://www.gnu.org/software/gawk/manual/html_node/FS-versus-FPAT.html

local regex = require 'regex'

local re = regex.new{
    pattern = '[^,]*',
    grammar = 'extended'
}

local s = {
    ',Arnold,,MyTown,MyState,12345-6789,',
    'Robbins,Arnold,,MyTown,MyState,12345-6789,',
    ',Arnold,,MyTown,MyState,12345-6789,USA'
}

for _, si in ipairs(s) do
    for k, v in ipairs(regex.patsplit(re, si)) do
        print(k, v)
    end
end

print '--'

for _, si in ipairs(s) do
    si = byte_span.append(si)
    for k, v in ipairs(regex.patsplit(re, si)) do
        print(k, v)
    end
end
