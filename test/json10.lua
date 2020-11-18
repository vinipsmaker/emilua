local json = require('json')

local x = json.decode('{"foo": "1", "bar": {"foo": {"": {}, " ": [], "bar": 2}}, "baz": 3}')
print(json.is_array(x))
print(x.foo)
print(x.bar.foo.bar)
print(x.baz)
