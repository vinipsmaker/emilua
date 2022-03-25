-- We don't follow Python behavior. We follow GAWK's.
--
-- From GAWK's rationale[1]:
--
-- > In order to distinguish one field from another, there must be a non-empty
-- > separator between each field. This makes intuitive sense—otherwise one
-- > could not distinguish fields from separators.
-- >
-- > Thus, regular expression matching as done when splitting fields with FS is
-- > not allowed to match the null string; it must always match at least one
-- > character, in order to be able to proceed through the entire record.
--
-- Unlike Python, there's patsplit() if you need to possibly match empty
-- separators. Again, from GAWK's rationale[1]:
--
-- > On the other hand, regular expression matching with FPAT can match the null
-- > string, and the non-matching intervening characters function as the
-- > separators.
--
-- [1] https://www.gnu.org/software/gawk/manual/html_node/FS-versus-FPAT.html

local byte_span = require 'byte_span'
local regex = require 'regex'

local re = regex.new{
    pattern = '^',
    grammar = 'extended'
}

local s = 'Robbins,Arnold,"1234 A Pretty Street, NE",MyTown,MyState,12345-6789,USA'

for k, v in ipairs(regex.split(re, s)) do
    print(k, v)
end

s = byte_span.append(s)

for k, v in ipairs(regex.split(re, s)) do
    print(k, v)
end
