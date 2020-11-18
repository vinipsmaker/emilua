local json = require('json')

local o = {}
local b = json.decode(json.encode({a = o, b = o}))
print(json.encode(b.a))
print(json.encode(b.b))
