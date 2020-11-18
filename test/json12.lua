local json = require('json')

local o = {}
o.o = o
print(json.encode(o))
